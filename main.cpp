#include <SFML/Graphics.hpp>
#include <cstdint>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace fs = std::experimental::filesystem;

constexpr int WIDTH_ADJUST_DIVIDER = 2;
constexpr int HEIGHT_ADJUST_DIVIDER = 2;
constexpr int IMG_MIN_WIDTH = 2 * WIDTH_ADJUST_DIVIDER;
constexpr int IMG_MIN_HEIGHT = 2 * HEIGHT_ADJUST_DIVIDER;

class ImgSpecs {
public:
  ImgSpecs( int width, int height ) : m_width( width ), m_height( height ), m_pixelCount( m_width * m_height ) {}
  size_t getWidth() const { return m_width; }
  size_t getHeight() const { return m_height; }
  size_t getPixelCount() const { return m_pixelCount; }

private:
  size_t m_width;
  size_t m_height;
  size_t m_pixelCount; // to avoid recalculation
};

using Img = std::vector<uint8_t>;

Img createImage( const ImgSpecs &img_specs ) { return Img( img_specs.getPixelCount() ); }

template <class G, class D1, class D2, class D3>
void mutate( Img &spec, const ImgSpecs &img_specs, G &generator, D1 &color_dist, D2 &width_dist, D3 &height_dist ) {
  int width = width_dist( generator );
  int height = height_dist( generator );
  auto x_dist = std::uniform_int_distribution<size_t>{0, img_specs.getWidth() - width};
  auto x = x_dist( generator );
  auto y_dist = std::uniform_int_distribution<size_t>{0, img_specs.getHeight() - height};
  auto y = y_dist( generator );
  auto color = color_dist( generator );

  for ( auto row = y; row < y + height; ++row ) {
    for ( auto col = x; col < x + width; ++col ) {
      // cast to ensure the calculations are being done on wide enough types
      spec[ row * img_specs.getWidth() + col ] = static_cast<decltype( color )>(
          ( static_cast<int>( spec[ row * img_specs.getWidth() + col ] ) + color ) >> 1 );
    }
  }
}

double calculateScore( const Img &sp, const Img &ideal, const ImgSpecs &img_specs ) {
  double sc = 0.0;
  for ( size_t j = 0; j < img_specs.getHeight(); j++ ) {
    for ( size_t i = 0; i < img_specs.getWidth(); i++ ) {
      double a = sp[ j * img_specs.getWidth() + i ];
      double b = ideal[ j * img_specs.getWidth() + i ];
      sc += ( a - b ) * ( a - b );
    }
  }

  return sc;
}

void updateTexture( sf::Texture &texture, const Img &data ) {
  Img tmp( data.size() * 4 ); // texture expects 32 bppx (4 Bppx)
  for ( decltype( data.size() ) i = 0; i < data.size(); ++i ) {
    tmp[ 4 * i ] = data[ i ];
    tmp[ 4 * i + 1 ] = data[ i ];
    tmp[ 4 * i + 2 ] = data[ i ];
    tmp[ 4 * i + 3 ] = 255;
  }
  texture.update( tmp.data() );
}

ImgSpecs makeImageSpecs( int width, int height ) {
  if ( width < IMG_MIN_WIDTH ) {
    std::cerr << "m_width must be an integer greater or equal " << IMG_MIN_WIDTH << "\n";
    exit( 1 );
  }

  if ( height < IMG_MIN_HEIGHT ) {
    std::cerr << "m_height must be an integer greater or equal " << IMG_MIN_HEIGHT << "\n";
    exit( 1 );
  }

  return ImgSpecs{width, height};
}

int main( int argc, char **argv ) {

  if ( argc < 4 ) {
    std::cerr << "Usage:\n\t" << argv[ 0 ] << " <m_width> <m_height> <file>\n";
    exit( 1 );
  }

  const auto img_specs = makeImageSpecs( atoi( argv[ 1 ] ), atoi( argv[ 2 ] ) );
  const auto file_path = fs::path{argv[ 3 ]};

  // Init random stuff
  auto generator = std::mt19937{std::random_device{}()};
  auto color_dist = std::uniform_int_distribution<uint8_t>{0, 255};
  auto width_dist = std::uniform_int_distribution<size_t>{1, img_specs.getWidth() / WIDTH_ADJUST_DIVIDER};
  auto height_dist = std::uniform_int_distribution<size_t>{1, img_specs.getHeight() / HEIGHT_ADJUST_DIVIDER};

  // read the image
  std::error_code error_code;
  auto file_size = fs::file_size( file_path, error_code );
  if ( error_code ) {
    std::cerr << "Error: " << error_code.message() << "\n";
    exit( 1 );
  } else if ( file_size != img_specs.getPixelCount() ) {
    std::cerr << "Incorrect image file size. Expected " << img_specs.getPixelCount() << " got " << file_size << "\n";
    exit( 1 );
  }

  auto img_stream = std::ifstream{file_path.string(), std::ios::binary};
  if ( !img_stream ) {
    std::cerr << "Unable to open file " << file_path.string() << "\n";
    exit( 1 );
  }

  auto ideal = createImage( img_specs );
  ideal.resize( img_specs.getPixelCount() );
  img_stream.read( reinterpret_cast<char *>( ideal.data() ), img_specs.getPixelCount() );
  img_stream.close();

  // create the window
  sf::RenderWindow window( sf::VideoMode( img_specs.getWidth() * 2, img_specs.getHeight() ), "Genetic Image Painter" );
  sf::Vector2u dim = window.getSize();
  std::cout << dim.x << 'x' << dim.y << std::endl;

  // make current_best all black at the  beginning
  auto current_best = createImage( img_specs );
  current_best.assign( current_best.size(), 0 );

  sf::Texture original_texture;
  original_texture.create( img_specs.getWidth(), img_specs.getHeight() );
  updateTexture( original_texture, ideal );

  sf::Sprite ideal_result{original_texture};

  sf::Texture texture;
  texture.create( img_specs.getWidth(), img_specs.getHeight() );

  sf::Sprite current_result{texture};
  current_result.setPosition( img_specs.getWidth(), 0 ); // make the sprite appear right of the original

  double best_score = std::numeric_limits<double>::max();

  // run the program as long as the window is open
  while ( window.isOpen() ) {
    // check all the window's events that were triggered since the last
    // iteration of the loop
    sf::Event event;
    while ( window.pollEvent( event ) ) {
      // "close requested" event: we close the window
      if ( event.type == sf::Event::Closed )
        window.close();
    }

    auto current = current_best;
    mutate( current, img_specs, generator, color_dist, width_dist, height_dist );
    double newScore = calculateScore( current, ideal, img_specs );

    if ( newScore < best_score ) {
      current_best.swap( current );
      updateTexture( texture, current_best );
      best_score = newScore;
    }

    // clear the window with black color
    window.clear( sf::Color::Black );

    // draw everything here...
    // window.draw(...);
    window.draw( ideal_result );
    window.draw( current_result );

    // end the current frame
    window.display();
  }

  return 0;
}
