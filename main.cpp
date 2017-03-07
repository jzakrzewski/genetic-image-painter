#include <SFML/Graphics.hpp>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

struct ImgSpecs {
  size_t width;
  size_t height;
  size_t pix_cnt; // to avoid recalculation
};

using Img = std::vector<uint8_t>;

Img createImage( const ImgSpecs &img_specs ) { return Img( img_specs.pix_cnt ); }

template <class G, class D1, class D2, class D3>
void mutate( Img &spec, const ImgSpecs &img_specs, G &generator, D1 &color_dist, D2 &width_dist, D3 &height_dist ) {
  int width = width_dist( generator );
  int height = height_dist( generator );
  auto x_dist = std::uniform_int_distribution<size_t>{0, img_specs.width - width};
  auto x = x_dist( generator );
  auto y_dist = std::uniform_int_distribution<size_t>{0, img_specs.height - height};
  auto y = y_dist( generator );
  auto color = color_dist( generator );

  for ( auto row = y; row < y + height; ++row ) {
    for ( auto col = x; col < x + width; ++col ) {
      // cast to ensure the calculations are being done on wide enough types
      spec[ row * img_specs.width + col ] =
          static_cast<decltype( color )>( ( static_cast<int>( spec[ row * img_specs.width + col ] ) + color ) >> 1 );
    }
  }
}

double score_me( const Img &sp, const Img &ideal, const ImgSpecs &img_specs ) {
  double sc = 0.0;
  for ( size_t j = 0; j < img_specs.height; j++ ) {
    for ( size_t i = 0; i < img_specs.width; i++ ) {
      double a = sp[ j * img_specs.width + i ];
      double b = ideal[ j * img_specs.width + i ];
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

int main( int argc, char **argv ) {

  if ( argc < 4 ) {
    std::cerr << "Usage:\n\t" << argv[ 0 ] << " <width> <height> <file>";
    exit( 1 );
  }

  // FIXME: Actually check those if they are not negative
  const auto img_width = static_cast<size_t>( atoi( argv[ 1 ] ) );
  const auto img_height = static_cast<size_t>( atoi( argv[ 2 ] ) );
  const auto img_specs = ImgSpecs{img_width, img_height, img_width * img_height};
  const auto file_name = std::string{argv[ 3 ]};

  // Init random stuff
  auto generator = std::mt19937{std::random_device{}()};
  auto color_dist = std::uniform_int_distribution<uint8_t>{0, 255};
  auto width_dist = std::uniform_int_distribution<size_t>{1, img_specs.width / 2};
  auto height_dist = std::uniform_int_distribution<size_t>{1, img_specs.height / 2};

  // read the image
  auto img_stream = std::ifstream{file_name, std::ios::binary};
  auto ideal = createImage( img_specs );
  ideal.resize( img_specs.pix_cnt );
  img_stream.read( reinterpret_cast<char *>( ideal.data() ), img_specs.pix_cnt );
  img_stream.close();

  // create the window
  sf::RenderWindow window( sf::VideoMode( img_specs.width * 2, img_specs.height ), "Genetic Image Painter" );
  sf::Vector2u dim = window.getSize();
  std::cout << dim.x << 'x' << dim.y << std::endl;

  // make current_best all black at the  beginning
  auto current_best = createImage( img_specs );
  current_best.assign( current_best.size(), 0 );

  sf::Texture original_texture;
  original_texture.create( img_specs.width, img_specs.height );
  updateTexture( original_texture, ideal );

  sf::Sprite ideal_result{original_texture};

  sf::Texture texture;
  texture.create( img_specs.width, img_specs.height );

  sf::Sprite current_result{texture};
  current_result.setPosition( img_specs.width, 0 ); // make the sprite appear right of the original

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
    double newScore = score_me( current, ideal, img_specs );

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
