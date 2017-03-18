#include <SFML/Graphics.hpp>
#include <chrono>
#include <cstdint>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace fs = std::experimental::filesystem;

double constexpr sqrtNewtonRaphson( double x, double curr, double prev ) {
  return curr == prev ? curr : sqrtNewtonRaphson( x, 0.5 * ( curr + x / curr ), curr );
}

double constexpr ct_sqrt( double x ) {
  return x >= 0 && x < std::numeric_limits<double>::infinity() ? sqrtNewtonRaphson( x, x, 0 )
                                                               : std::numeric_limits<double>::quiet_NaN();
}

constexpr int WIDTH_ADJUST_DIVIDER = 2;
constexpr int HEIGHT_ADJUST_DIVIDER = 2;
constexpr int IMG_MIN_WIDTH = 2 * WIDTH_ADJUST_DIVIDER;
constexpr int IMG_MIN_HEIGHT = 2 * HEIGHT_ADJUST_DIVIDER;
constexpr size_t POPULATION_SIZE = 100;
constexpr double CROSS_RATE = 0.98;
constexpr double MUTATION_RATE = 0.4;
// highly advanced maths to find how many specimens to cross to create X% of the new population
// it's POPULATION_SIZE * CROSS_RATE = n * (n+1)/2 solved for n
constexpr size_t CROSS_COUNT = static_cast<size_t>( ( ct_sqrt( ( POPULATION_SIZE * CROSS_RATE ) * 8 + 1 ) - 1 ) / 2 );
constexpr size_t SURVIVORS_COUNT = POPULATION_SIZE - CROSS_COUNT * ( CROSS_COUNT + 1 ) / 2;

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

class Specimen {
public:
  Specimen( const ImgSpecs &img_specs )
      : m_imgSpecs{img_specs}, m_image( img_specs.getPixelCount() ), m_score{std::numeric_limits<size_t>::max()} {}

  Specimen( const Specimen & ) = default;
  Specimen( Specimen && ) = default;
  Specimen &operator=( const Specimen & ) = default;
  Specimen &operator=( Specimen && ) = default;

  Img::value_type &px( size_t row, size_t col ) { return m_image[ row * m_imgSpecs.getWidth() + col ]; }
  Img::value_type px( size_t row, size_t col ) const { return m_image[ row * m_imgSpecs.getWidth() + col ]; }
  Img::value_type &operator[]( size_t i ) { return m_image[ i ]; }
  Img::value_type operator[]( size_t i ) const { return m_image[ i ]; }

  template <class G, class D1, class D2, class D3>
  void mutate( G &generator, D1 &color_dist, D2 &width_dist, D3 &height_dist ) {
    int width = width_dist( generator );
    int height = height_dist( generator );
    auto x_dist = std::uniform_int_distribution<size_t>{0, m_imgSpecs.getWidth() - width};
    auto x = x_dist( generator );
    auto y_dist = std::uniform_int_distribution<size_t>{0, m_imgSpecs.getHeight() - height};
    auto y = y_dist( generator );
    auto color = color_dist( generator );

    for ( auto row = y; row < y + height; ++row ) {
      for ( auto col = x; col < x + width; ++col ) {
        // cast to ensure the calculations are being done on wide enough types
        px( row, col ) = static_cast<decltype( color )>( ( static_cast<int>( px( row, col ) ) + color ) >> 1 );
      }
    }
  }

  void fill( uint8_t color ) { std::fill( m_image.begin(), m_image.end(), color ); }

  uint8_t *getRaw() { return m_image.data(); }

  const ImgSpecs &specs() const { return m_imgSpecs; }

  size_t rescore( const Specimen &ideal ) {
    m_score = 0;
    auto me = m_image.data();
    auto it = ideal.m_image.data();
    for ( size_t i = 0; i < m_imgSpecs.getPixelCount(); ++i ) {
      m_score += ( me[ i ] - it[ i ] ) * ( me[ i ] - it[ i ] );
    }
    return m_score;
  }

  Specimen mate( const Specimen &mate_partner ) const {
    auto child = Specimen{m_imgSpecs};
    for ( size_t i = 0; i < m_imgSpecs.getPixelCount(); ++i ) {
      child[ i ] = static_cast<Img::value_type>( ( static_cast<int>( m_image[ i ] ) + mate_partner[ i ] ) >> 1 );
    }
    return child;
  }

  size_t score() const { return m_score; }

private:
  ImgSpecs m_imgSpecs;
  Img m_image;
  size_t m_score;
};

bool operator<( const Specimen &left, const Specimen &right ) { return left.score() < right.score(); }

using Generation = std::vector<Specimen>;

void updateTexture( sf::Texture &texture, const Specimen &specimen ) {
  auto px_count = specimen.specs().getPixelCount();
  auto tmp = std::make_unique<uint8_t[]>( px_count * 4 ); // texture expects 32 bppx (4 Bppx)
  for ( decltype( px_count ) i = 0; i < px_count; ++i ) {
    tmp[ 4 * i ] = specimen[ i ];
    tmp[ 4 * i + 1 ] = specimen[ i ];
    tmp[ 4 * i + 2 ] = specimen[ i ];
    tmp[ 4 * i + 3 ] = 255;
  }
  texture.update( tmp.get() );
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
  auto mutation_dist = std::bernoulli_distribution{MUTATION_RATE};

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

  auto ideal = Specimen{img_specs};
  ideal.fill( 0 ); // make it black

  // create a population, start with all black
  auto population = Generation{POPULATION_SIZE, ideal};
  // and do few random mutations to make it diverse
  std::for_each( population.begin(), population.end(),
                 [&]( auto &&specimen ) { specimen.mutate( generator, color_dist, width_dist, height_dist ); } );

  img_stream.read( reinterpret_cast<char *>( ideal.getRaw() ), img_specs.getPixelCount() );
  img_stream.close();

  // create the window
  sf::RenderWindow window( sf::VideoMode( img_specs.getWidth() * 2, img_specs.getHeight() ), "Genetic Image Painter" );
  sf::Vector2u dim = window.getSize();
  std::cout << dim.x << 'x' << dim.y << std::endl;

  sf::Texture original_texture;
  original_texture.create( img_specs.getWidth(), img_specs.getHeight() );
  updateTexture( original_texture, ideal );

  sf::Sprite ideal_result{original_texture};

  sf::Texture texture;
  texture.create( img_specs.getWidth(), img_specs.getHeight() );

  sf::Sprite current_result{texture};
  current_result.setPosition( img_specs.getWidth(), 0 ); // make the sprite appear right of the original

  size_t counter = 0;

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

    auto start = std::chrono::system_clock::now();

    auto new_gen = Generation{};
    new_gen.reserve( POPULATION_SIZE );

    auto gen_created = std::chrono::system_clock::now();

    auto partners_end = std::next( population.begin(), CROSS_COUNT + 1 );

    std::copy_n( population.begin(), SURVIVORS_COUNT, std::back_inserter( new_gen ) );

    auto gen_copied = std::chrono::system_clock::now();

    for ( auto specimen = population.begin(); specimen != partners_end; ++specimen ) {
      std::transform( std::next( specimen ), partners_end, std::back_inserter( new_gen ),
                      [&]( auto &&mate_partner ) { return specimen->mate( mate_partner ); } );
    }

    auto gen_crossed = std::chrono::system_clock::now();

    std::for_each( std::next( new_gen.begin(), SURVIVORS_COUNT + 1 ), new_gen.end(), [&]( auto &&specimen ) {
      if ( mutation_dist( generator ) ) {
        specimen.mutate( generator, color_dist, width_dist, height_dist );
      }
    } );

    auto gen_mutated = std::chrono::system_clock::now();

    std::for_each( new_gen.begin(), new_gen.end(), [&]( auto &&specimen ) { specimen.rescore( ideal ); } );

    auto gen_rescored = std::chrono::system_clock::now();

    std::sort( new_gen.begin(), new_gen.end() );

    auto gen_sorted = std::chrono::system_clock::now();

    new_gen.swap( population );
    updateTexture( texture, population[ 0 ] );
    std::cout << counter++ << ": " << ( gen_created - start ).count() << ' ' << ( gen_copied - gen_created ).count()
              << ' ' << ( gen_crossed - gen_copied ).count() << ' ' << ( gen_mutated - gen_crossed ).count() << ' '
              << ( gen_rescored - gen_mutated ).count() << ' ' << ( gen_sorted - gen_rescored ).count() << ' '
              << ( std::chrono::system_clock::now() - gen_sorted ).count() << std::endl;
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
