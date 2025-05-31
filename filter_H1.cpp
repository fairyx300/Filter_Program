// filter.cpp - BMP Image Filtering Program
// This program loads BMP images, applies user-selected filters and saves the modified image.

// libaries
#include <string>
#include <string.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
using std::string;

// data type aliases
typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;

//bitmap header struct
// ref: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader?redirectedfrom=MSDN

#pragma pack(push, 1)
struct BitmapFileHeader {
    WORD bfType;        // Magic number for file
    DWORD bfSize;      // Size of the file in bytes
    WORD bfReserved1;  // Reserved, must be 0
    WORD bfReserved2;  // Reserved, must be 0
    DWORD bfOffBits;   // Offset to start of pixel data
};
#pragma pack(pop)
//bitmap info struct
// ref: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader?redirectedfrom=MSDN
#pragma pack(push, 1)
struct BitmapInfoHeader {
    DWORD biSize;          // Size of this header
    LONG biWidth;          // Width of the bitmap
    LONG biHeight;         // Height of the bitmap
    WORD biPlanes;        // Number of color planes
    WORD biBitCount;      // Bits per pixel
    DWORD biCompression;  // Compression type
    DWORD biSizeImage;    // Size of the image data
    LONG biXPelsPerMeter;  // Horizontal resolution
    LONG biYPelsPerMeter;  // Vertical resolution
    DWORD biClrUsed;      // Number of colors used
    DWORD biClrImportant; // Important colors
};
#pragma pack(pop)

// pixel data struct 
struct Pixeldata{
    BYTE B;
    BYTE G;
    BYTE R;
};

// image struct
struct ImageDetails {
    int width;
    int height;
    Pixeldata** pixels;
};

// program states struct
struct program_states {
    int selected_filter;
    int filter_strength;
    string file_path;
};

// info for filters
struct filter_option{
    std::string filter_type;
    bool has_parameters;
};

// the number of filters
const int NUM_FILTERS = 9;

// define filter types
const filter_option FILTER_TYPES[NUM_FILTERS] = {
    {"No Filter", false},
    {"Grayscale", false},
    {"Sepia", true},
    {"Flip", false},
    {"Gaussian Blur", true},
    {"Sharpen", true},
    {"Edge Detection", false},
    {"Noise Reduction", true},
    {"ASCII", false}
};

struct AsciiFilter {
    int width;
    int height;
    char** pixels;
};

// standard deviation for gaussian blur σ ≈ 1
const float GAUSSIAN_KERNEL[3][3] = {
    {1.0f/16, 2.0f/16, 1.0f/16},
    {2.0f/16, 4.0f/16, 2.0f/16},
    {1.0f/16, 2.0f/16, 1.0f/16}
};

// function and procedure declaration
void initialise_program_states(program_states& states);
void selectFilter(program_states& states, ImageDetails& image);
void ascending_sort(BYTE colour[], int num_elements);
void subtract_images(const ImageDetails& a, const ImageDetails& b, ImageDetails& result);
void multiply_image(ImageDetails& img, float scalar);
void add_images(const ImageDetails& a, const ImageDetails& b, ImageDetails& result);
void freeImage(ImageDetails& image);
bool convert_to_bmp(string filepath);
string get_filename(const string& filepath);
string replace_ext_with_bmp(const string& filename);
Pixeldata** copy_pixels(Pixeldata** source, int height, int width);
int check_and_read_file(string file_path, ImageDetails& image, BitmapFileHeader& file_header, BitmapInfoHeader& info_header);
void make_output_file(string output_file_name, ImageDetails image,  string file_path, BitmapFileHeader file_header, BitmapInfoHeader info_header);
string get_directory(string file_path);
string strip_extension(string filename);
void applyGrayscale(ImageDetails& image);
void applySepia(ImageDetails& image, int filter_strength);
void applyFlip(ImageDetails& image);
void applyGaussianBlur(ImageDetails& image);
void applySharpen(ImageDetails& image, int filter_strength);
void applyEdgeDetection(ImageDetails& image);
void applyNoiseReduction(ImageDetails& image, int filter_strength);
AsciiFilter* ASCII_filter(ImageDetails& image);
void freeAsciiImage(AsciiFilter* ascii_image);
void changeImageSize(ImageDetails& image, int new_size);
void make_ascii(program_states& states, ImageDetails& image);
void saveAsciiImage(AsciiFilter* ascii_image, string& filename);

int main() {
    // initialise program 
    program_states states;
    initialise_program_states(states);
    ImageDetails image;
    BitmapFileHeader file_header;
    BitmapInfoHeader info_header;
    string file_path;


    int result = 0;

    // get the file path from the user
    do {
        std::cout << "Enter the file path: ";
        std::getline(std::cin, file_path);
        if (file_path.empty()) {
            std::cerr << "Error: No file path provided" << std::endl;
            return 1;
        }

        // check if the file path is valid
        result = check_and_read_file (file_path, image, file_header, info_header);
        if (result == 0 && file_path.substr(file_path.size() - 4) != ".bmp") {
            // If conversion happened, update file_path to the new BMP
            file_path = replace_ext_with_bmp(get_filename(file_path));
        }
        if (result != 0) {
            std::cerr << "Error: Invalid file" << std::endl;
        }
    } while (result != 0);

    // get filter type from user 
    while (states.selected_filter < 1 || states.selected_filter >= NUM_FILTERS) {
        std::cout << "Select a filter type (1 - 8): \n";
        for (int i = 1; i < NUM_FILTERS; i++) {
            std::cout << i << ": " << FILTER_TYPES[i].filter_type << std::endl;
        }
        int filter_type;
        std::cin >> filter_type;

        if (filter_type < 1 || filter_type >= NUM_FILTERS) {
            std::cerr << "Error: Invalid filter type" << std::endl;
            continue;
        }

        states.selected_filter = filter_type;
    }   

    // get filter strength from user
    if (FILTER_TYPES[states.selected_filter].has_parameters) {
        do {
            std::cout << "Enter the filter strength (1 - 100): ";
            std::cin >> states.filter_strength;
            if (states.filter_strength < 1 || states.filter_strength > 100) {
                std::cerr << "Error: Invalid filter strength" << std::endl;
            }
        } while (states.filter_strength < 1 || states.filter_strength > 100);
    }

    states.file_path = file_path;

    // apply the selected filter
    selectFilter(states, image);
    if (states.selected_filter != 8){
        string filename = strip_extension(get_filename(file_path));
        string output_file = filename + "_" + FILTER_TYPES[states.selected_filter].filter_type + ".bmp";
        make_output_file(output_file, image, file_path, file_header, info_header);
    }
    freeImage(image);
    return 0;
}

void initialise_program_states(program_states& states) {
    // initialize the program states
    states.selected_filter = 0;
    states.file_path = "";
}

// stays in the main 
void selectFilter(program_states& states, ImageDetails& image) {
    // apply the selected filter
    switch (states.selected_filter) {
        case 0:
            // no filter
            break;
        case 1:
            // grayscale
            applyGrayscale(image);
            break;
        case 2:
            // sepia
            applySepia(image, states.filter_strength);
            break;
        case 3:
            // flip
            applyFlip(image);
            break;
        case 4:
            // gaussian blur
            for (int i = 0; i < states.filter_strength; i++){
                applyGaussianBlur(image);
            }
            break;
        case 5:
            // sharpen
            applySharpen(image, states.filter_strength);
            break;
        case 6:
            // edge detection
            applyEdgeDetection(image);
            break;
        case 7:
            // noise reduction
            applyNoiseReduction(image, states.filter_strength);
            break;
        case 8:
            // run ascii process
            make_ascii(states, image);
            break;
        default:
            std::cerr << "Error: Invalid filter type" << std::endl;
    }
}

void make_ascii(program_states& states, ImageDetails& image){
    //get size from user
    int new_size;
    do {
        std::cout << "Enter image size: ";
        std::cin >> new_size;
        if (new_size <= 0 || new_size > image.width || new_size > image.height) {
            std::cerr << "Error: Invalid new size." << std::endl;
        }
    } while (new_size <= 0 || new_size > image.width || new_size > image.height);


    //change the image size
    changeImageSize(image, new_size);

    // apply filters here
    AsciiFilter* ascii_image = ASCII_filter(image);
    if (ascii_image == NULL) {
        std::cerr << "Error: Could not allocate memory for ASCII image" << std::endl;
        return;
    }

    std::cout << "ASCII image created successfully!" << std::endl;

    // save ascii
    string directory = get_directory(states.file_path);
    string filename = strip_extension(get_filename(states.file_path));
    string output_file = filename + "_" + FILTER_TYPES[states.selected_filter].filter_type + ".txt";
    string output_path = directory.empty() ? output_file : directory + "/" + output_file;
    saveAsciiImage(ascii_image, output_path);
    std::cout << "ASCII image saved to: " << output_path << std::endl;

    // free memory
    freeAsciiImage(ascii_image);
}


void ascending_sort(BYTE colour[], int num_elements){
    for (int i = 0; i < num_elements; i++){
        for (int j = i + 1; j < num_elements; j++) {
            // compare elements, smallest on left
            if (colour[j] < colour[i]){
                // swqp
                BYTE temp = colour[i];
                colour[i] = colour[j];
                colour[j] = temp;
            }
        }
    }
}

// Subtract two images pixel-wise
void subtract_images(const ImageDetails& a, const ImageDetails& b, ImageDetails& result) {
    // loop through x and y, a - b for each color channel
    for (int y = 0; y < a.height; y++) {
        for (int x = 0; x < a.width; x++) {
            result.pixels[y][x].R = a.pixels[y][x].R - b.pixels[y][x].R;
            result.pixels[y][x].G = a.pixels[y][x].G - b.pixels[y][x].G;
            result.pixels[y][x].B = a.pixels[y][x].B - b.pixels[y][x].B;
        }
    }
}

// Multiply image by scalar
void multiply_image(ImageDetails& image, float scalar) {
    // loop through x and y, multipling each color chanel by the scalar clamping between 0 and 255
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            image.pixels[y][x].R = std::clamp(int(image.pixels[y][x].R * scalar), 0, 255);
            image.pixels[y][x].G = std::clamp(int(image.pixels[y][x].G * scalar), 0, 255);
            image.pixels[y][x].B = std::clamp(int(image.pixels[y][x].B * scalar), 0, 255);
        }
    }
}

// Add two images pixel-wise
void add_images(const ImageDetails& a, const ImageDetails& b, ImageDetails& result) {
    // loop through x and y, a + b for each color channel
    for (int y = 0; y < a.height; y++) {
        for (int x = 0; x < a.width; x++) {
            result.pixels[y][x].R = std::clamp(int(a.pixels[y][x].R + b.pixels[y][x].R), 0, 255);
            result.pixels[y][x].G = std::clamp(int(a.pixels[y][x].G + b.pixels[y][x].G), 0, 255);
            result.pixels[y][x].B = std::clamp(int(a.pixels[y][x].B + b.pixels[y][x].B), 0, 255);
        }
    }
}


// freeing allocated memory
void freeImage(ImageDetails& image) {
    if (image.pixels != nullptr) {
        for (int i = 0; i < image.height; i++) {
            delete[] image.pixels[i];
        }
        delete[] image.pixels;
        image.pixels = nullptr;
    }
}

bool convert_to_bmp(string filepath){
    string filename = get_filename(filepath);
    string outname = replace_ext_with_bmp(filename);

    // Only convert if not already .bmp
    if (filepath.size() >= 4 && filepath.substr(filepath.size() - 4) == ".bmp") {
        std::cout << "File is already a BMP: " << filepath << std::endl;
        return true;
    }

    // convert to 24 bit bmp using command prompt 
    string command = "convert \"" + filepath + "\" -depth 8 -type TrueColor BMP3:\"" + outname + "\"";
    int result = system(command.c_str());

    if (result == 0) {
        std::cout << "Image converted successfully: " << outname << std::endl;
        return true;
    } else {
        // error message
        std::cerr << "Image conversion failed with code: " << result << std::endl;
        return false;
    }
}

string get_filename(const string& filepath) {
    // extract results sfter the last /
    size_t last_slash = filepath.find_last_of("/\\");
    if (last_slash == string::npos) return filepath;
    return filepath.substr(last_slash + 1);
}

string replace_ext_with_bmp(const string& filename) {
    // replace the extentions with .bmp
    size_t last_dot = filename.find_last_of('.');
    if (last_dot == string::npos) return filename + ".bmp";
    return filename.substr(0, last_dot) + ".bmp";
}

Pixeldata** copy_pixels(Pixeldata** source, int height, int width) {
    // allocate memory for height
    Pixeldata** destination = new Pixeldata*[height];
    // loop through y allocating memory for x
    for (int y = 0; y < height; y++) {
        destination[y] = new Pixeldata[width];
        for (int x = 0; x < width; x++) {
            // copy the pixel data
            destination[y][x] = source[y][x];
        }
    }
    // return copied pixels 
    return destination;
}


// checks and reads file
int check_and_read_file(string file_path, ImageDetails& image, BitmapFileHeader& file_header, BitmapInfoHeader& info_header) {
    
    const char* in_file_name = file_path.c_str();

    // open the file in read bytes mode
    FILE* in_file = fopen(in_file_name, "rb");
    if (in_file == NULL) {
        std::cerr << "Error: Could not open file " << in_file_name << std::endl;
        return 1;
    }

    // read the file header
    fread(&file_header, sizeof(BitmapFileHeader), 1, in_file);

    // read the info header
    fread(&info_header, sizeof(BitmapInfoHeader), 1, in_file);

    // check if the file is a valid 24bit bitmap
    if (file_header.bfType != 0x4D42 || info_header.biBitCount != 24 || info_header.biCompression != 0) {

        // debugs
        // std::cerr << "Error: File is not a valid bitmap" << std::endl;
        // std::cout << "bfType: " << std::hex << file_header.bfType << std::dec << std::endl;
        // std::cout << "bfOffBits: " << file_header.bfOffBits << std::endl;
        // std::cout << "biSize: " << info_header.biSize << std::endl;
        // std::cout << "biBitCount: " << info_header.biBitCount << std::endl;
        // std::cout << "biCompression: " << info_header.biCompression << std::endl;
        fclose(in_file);

        // Only attempt conversion if the file is not already a .bmp
        if (file_path.size() < 4 || file_path.substr(file_path.size() - 4) != ".bmp") {
            if (convert_to_bmp(file_path)) {
                // Try again with the new BMP file
                string new_bmp = replace_ext_with_bmp(get_filename(file_path));
                return check_and_read_file(new_bmp, image, file_header, info_header);
            } else {
                return 2;
            }
        } else {
            return 2;
        }
    }   

    // get the image dimensions
    image.width = info_header.biWidth;
    image.height = abs(info_header.biHeight);

    // allocate memory for image
    image.pixels = new Pixeldata*[image.height];
    for (int i = 0; i < image.height; i++) {
        image.pixels[i] = new Pixeldata[image.width];
    }

    // check if memory allocation was successful
    if (image.pixels == NULL) {
        std::cerr << "Error: Could not allocate memory for image" << std::endl;
        fclose(in_file);
        return 3;
    }

    // determine the padding
    int padding = (4 - (image.width * 3) % 4) % 4;

    // read the pixel data
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            fread(&image.pixels[y][x], sizeof(Pixeldata), 1, in_file);
        }
        // skip the padding
        fseek(in_file, padding, SEEK_CUR);
    }

    // close the file
    fclose(in_file);
    return 0;
}

// file
void make_output_file(string output_file_name, ImageDetails image,  string file_path, BitmapFileHeader file_header, BitmapInfoHeader info_header) {

    string directory = get_directory(file_path);
    // make the output file name
  
    if (output_file_name.empty()) {
        output_file_name = get_filename(file_path) + "_out";
        output_file_name = replace_ext_with_bmp(output_file_name);
    }


    string out_file_path;
    if (!directory.empty())
        out_file_path = directory + "/" + output_file_name;
    else
        out_file_path = output_file_name;

    std::ofstream out_file(out_file_path, std::ios::binary);
    
    if (!out_file) {
        std::cerr << "Error: Could not open output file " << output_file_name << std::endl;
        return;
    } else {
        out_file.write(reinterpret_cast<char*>(&file_header), sizeof(BitmapFileHeader));
        out_file.write(reinterpret_cast<char*>(&info_header), sizeof(BitmapInfoHeader));
        int padding = (4 - (image.width * 3) % 4) % 4;
        for (int y = 0; y < image.height; y++) {
            for (int x = 0; x < image.width; x++) {
                out_file.write(reinterpret_cast<char*>(&image.pixels[y][x]), 3); // BGR format
            }
            BYTE pad[3] = {0, 0, 0};
            out_file.write(reinterpret_cast<char*>(pad), padding);
        }

        out_file.close();
        std::cout << "Output file created: " << output_file_name << std::endl;
    }
}


string get_directory(string file_path) {
    // get the directory of the file
    size_t last_slash = file_path.find_last_of("/\\");
    if (last_slash != string::npos) {
        return file_path.substr(0, last_slash);
    }
    return "";
}

string strip_extension(string filename) {
    // remove file extention
    size_t last_dot = filename.find_last_of('.');
    if (last_dot == string::npos) return filename;
    return filename.substr(0, last_dot);
}


void applyGrayscale(ImageDetails& image) {
    // apply grayscale filter
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            int average = (image.pixels[y][x].R + image.pixels[y][x].G + image.pixels[y][x].B) / 3;
            image.pixels[y][x].R = average;
            image.pixels[y][x].G = average;
            image.pixels[y][x].B = average;
        }
    }
}

void applySepia(ImageDetails& image, int filter_strength) {
    // formula for sepia filter
    // newRed = 0.393 * R + 0.769 * G + 0.189 * B
    // newGreen = 0.349 * R + 0.686 * G + 0.168 * B
    // newBlue = 0.272 * R + 0.534 * G + 0.131 * B
    int temp_pixels[3];
    
    // apply sepia filter
    for (int  y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            temp_pixels[0] = round((image.pixels[y][x].R * 0.393) + (image.pixels[y][x].G * 0.769) + (image.pixels[y][x].B * 0.189));
            temp_pixels[1] = round((image.pixels[y][x].R * 0.349) + (image.pixels[y][x].G * 0.686) + (image.pixels[y][x].B * 0.168));
            temp_pixels[2] = round((image.pixels[y][x].R * 0.272) + (image.pixels[y][x].G * 0.534) + (image.pixels[y][x].B * 0.131));
            // clamp the values to 0-255
            temp_pixels[0] = std::min(255, temp_pixels[0]);
            temp_pixels[1] = std::min(255, temp_pixels[1]);
            temp_pixels[2] = std::min(255, temp_pixels[2]);

            // set the new pixel values
            image.pixels[y][x].R = temp_pixels[0];
            image.pixels[y][x].G = temp_pixels[1];
            image.pixels[y][x].B = temp_pixels[2];
        }
    }
}

void applyFlip(ImageDetails& image) {
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width / 2; x++) {
            std::swap(image.pixels[y][x], image.pixels[y][image.width - x - 1]);
        }
    }
}

void applyGaussianBlur(ImageDetails& image) {
    const int kernal_size = 3;
    const int offset = kernal_size / 2;

    Pixeldata** original_pixels = copy_pixels(image.pixels, image.height, image.width);

    for (int y = offset; y < image.height - offset; y++) {
        for (int x = offset; x < image.width - offset; x++) {
            float sumR = 0;
            float sumG = 0;
            float sumB = 0;

            for (int ky = -offset; ky <= offset; ky++) {
                for (int kx = -offset; kx <= offset; kx++) {

                    float weight = GAUSSIAN_KERNEL[ky + offset][kx + offset];
                    Pixeldata& pixel = original_pixels[y + ky][x + kx];

                    sumR += pixel.R * weight;
                    sumG += pixel.G * weight;
                    sumB += pixel.B * weight;
                }
            }

            // Clamp results to 0-255
            image.pixels[y][x].R = static_cast<BYTE>(std::clamp(sumR, 0.0f, 255.0f));
            image.pixels[y][x].G = static_cast<BYTE>(std::clamp(sumG, 0.0f, 255.0f));
            image.pixels[y][x].B = static_cast<BYTE>(std::clamp(sumB, 0.0f, 255.0f));
        }
    }

    for (int i = 0; i < image.height; i++) {
        delete[] original_pixels[i];
    }
    delete[] original_pixels;
}

void applySharpen(ImageDetails& image, int filter_strength) {
    // Deep copy for blurred image
    ImageDetails blured_image;
    blured_image.width = image.width;
    blured_image.height = image.height;
    blured_image.pixels = copy_pixels(image.pixels, image.height, image.width);

    applyGaussianBlur(blured_image);

    // Allocate sharpened mask
    ImageDetails sharpend_mask;
    sharpend_mask.width = image.width;
    sharpend_mask.height = image.height;
    sharpend_mask.pixels = copy_pixels(image.pixels, image.height, image.width);

    subtract_images(image, blured_image, sharpend_mask);
    multiply_image(sharpend_mask, filter_strength);
    add_images(image, sharpend_mask, image);

    // Clamp the values to 0-255
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            image.pixels[y][x].R = std::clamp(int(image.pixels[y][x].R), 0, 255);
            image.pixels[y][x].G = std::clamp(int(image.pixels[y][x].G), 0, 255);
            image.pixels[y][x].B = std::clamp(int(image.pixels[y][x].B), 0, 255);
        }
    }

    freeImage(blured_image);
    freeImage(sharpend_mask);
}

void applyEdgeDetection(ImageDetails& image) {
    int Gx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };

    int Gy[3][3] = {
        {1, 2, 1},
        {0,  0,  0},
        {-1,  -2,  -1}
    };

    // Deep copy
    ImageDetails temp_image;
    temp_image.pixels = copy_pixels(image.pixels, image.height, image.width);
    temp_image.height = image.height;
    temp_image.width = image.width;

    // Grayscale must update all channels
    applyGrayscale(temp_image);

    // Avoid borders
    for (int y = 1; y < image.height - 1; y++) {
        for (int x = 1; x < image.width - 1; x++) {
            int gx = 0, gy = 0;

            for (int ky = 0; ky < 3; ky++) {
                for (int kx = 0; kx < 3; kx++) {
                    int px = x + kx - 1;
                    int py = y + ky - 1;
                    BYTE intensity = temp_image.pixels[py][px].R;
                    gx += intensity * Gx[ky][kx];
                    gy += intensity * Gy[ky][kx];
                }
            }

            int magnitude = round(sqrt(gx * gx + gy * gy));
            if (magnitude > 255) magnitude = 255;
            if (magnitude < 0) magnitude = 0;

            image.pixels[y][x].R = magnitude;
            image.pixels[y][x].G = magnitude;
            image.pixels[y][x].B = magnitude;
        }
    }

    freeImage(temp_image); // only if it’s safe
}

// note this filter takes awhile due to the sorting
void applyNoiseReduction(ImageDetails& image, int filter_strength) {
    // kernal size must be odd
    if (filter_strength % 2 != 0) {
        filter_strength++;
        // use half strength
        filter_strength /= 2;
    }

    int kernel_size = 3 + filter_strength;
    int offset = kernel_size / 2;

    // Copy the original pixels
    Pixeldata** original_pixels = copy_pixels(image.pixels, image.height, image.width);

    // Loop over every pixel
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            BYTE Red[kernel_size * kernel_size];
            BYTE Green[kernel_size * kernel_size];
            BYTE Blue[kernel_size * kernel_size];

            int index = 0;

            // Collect pixel values in the kernel
            for (int ky = -offset; ky <= offset; ky++) {
                for (int kx = -offset; kx <= offset; kx++) {
                    int sample_y = y + ky;
                    int sample_x = x + kx;

                    // Check bounds
                    if (sample_y >= 0 && sample_y < image.height &&
                        sample_x >= 0 && sample_x < image.width) {
                        Pixeldata& pixel = original_pixels[sample_y][sample_x];
                        Red[index] = pixel.R;
                        Green[index] = pixel.G;
                        Blue[index] = pixel.B;
                        index++;
                    }
                }
            }

            // Sort the collected values
            ascending_sort(Red, index);
            ascending_sort(Green, index);
            ascending_sort(Blue, index);

            // Set the pixel to the median value
            if (index > 0) {
                int r = Red[index / 2];
                int g = Green[index / 2];
                int b = Blue[index / 2];
                        
                // Scale up for visibility (try 2, 4, or higher if needed)
                image.pixels[y][x].R = r;
                image.pixels[y][x].G = g;
                image.pixels[y][x].B = b;
            }

        }
    }

    // Free memory
    for (int i = 0; i < image.height; i++) {
        delete[] original_pixels[i];
    }
    delete[] original_pixels;
}


AsciiFilter* ASCII_filter(ImageDetails& image) {

    // apply grayscale filter
    applyGrayscale(image);

    // Find min and max grayscale values
    int min_val = 255, max_val = 0;
    for (int y = 0; y < image.height; y++)
        for (int x = 0; x < image.width; x++) {
            int val = image.pixels[y][x].R;
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
    // Stretch values
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            int val = image.pixels[y][x].R;
            int stretched = 255 * (val - min_val) / (max_val - min_val + 1);
            image.pixels[y][x].R = image.pixels[y][x].G = image.pixels[y][x].B = stretched;
        }
    }

    // apply ASCII filter
    AsciiFilter* ascii_image = new AsciiFilter;
    ascii_image->width = image.width;  
    ascii_image->height = image.height;
    // allocate memory for ASCII pixel data
    ascii_image->pixels = new char*[ascii_image->height];
    for (int i = 0; i < ascii_image->height; i++) {
        ascii_image->pixels[i] = new char[ascii_image->width];
    }

    const char* ascii_chars = " `.-':_,^=;><+!rc*/z?sLTv)J7(|Fi{C}fI31tlu[neoZ5Yxjya]2ESwqkP6h9d4VpOGbUAKXHm8RD#$Bg0MNWQ%&@";
    // 50 levels of brightness
    // const char* ascii_chars = "@%&#akdpwZ0LJYxvnrft/|()1{}[]?-_+~<>i!lI;:,^'. "; // 50 levels of brightness
    int num_chars = strlen(ascii_chars);
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            int index = (image.pixels[y][x].R * (num_chars - 1)) / 255;
            ascii_image->pixels[y][x] = ascii_chars[index];
        }
    }
    return ascii_image;
}

//save ASCII image to txt file
void saveAsciiImage(AsciiFilter* ascii_image, string& filename){
    // open file
    std::ofstream out(filename);

    if (!out) {
        std::cerr << "Error: Could not open file " << filename << " for writing." << std::endl;
        return;
    }

    for (int y = ascii_image->height - 1; y >= 0; y--) {
        for (int x = 0; x < ascii_image->width; x++) {
            out << ascii_image->pixels[y][x];
        }
        out << "\n";
    }

    // close file
    out.close();
}

// free memory
void freeAsciiImage(AsciiFilter* ascii_image) {
    for (int i = 0; i < ascii_image->height; i++) {
        delete[] ascii_image->pixels[i];
    }
    delete[] ascii_image->pixels;
    delete ascii_image;
}

void changeImageSize(ImageDetails& image, int new_width) {
    // adjust for font
    float aspect_ratio = 0.4f; 
    // calculate height based on aspect ratio
    int new_height = static_cast<int>(image.height * (new_width / (float)image.width) * aspect_ratio);

    // check if the new deminsions are valid
    if (new_width > image.width || new_height > image.height) {
        std::cerr << "Error: New size is larger than original image dimensions." << std::endl;
        return;
    }

    //make kernel size
    int kernal_size_x = image.width / new_width;
    int kernal_size_y = image.height / new_height;
    if (kernal_size_x <= 0 || kernal_size_y <= 0) {
        std::cerr << "Error: Kernel size is zero or negative. New size too large for image." << std::endl;
        return;
    }

    // allocate memory for new pixels
    Pixeldata** new_pixels = new Pixeldata*[new_height];
    for (int i = 0; i < new_height; i++) {
        new_pixels[i] = new Pixeldata[new_width];
    }

    // iterate through x and y 
    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            int sumR = 0, sumG = 0, sumB = 0, count = 0;
            for (int ky = 0; ky < kernal_size_y; ky++) {
                for (int kx = 0; kx < kernal_size_x; kx++) {
                    // move the pixels scaling by kernel size
                    int sourceY = y * kernal_size_y + ky;
                    int sourceX = x * kernal_size_x + kx;
                    if (sourceY < image.height && sourceX < image.width) {
                        sumR += image.pixels[sourceY][sourceX].R;
                        sumG += image.pixels[sourceY][sourceX].G;
                        sumB += image.pixels[sourceY][sourceX].B;
                        count++;
                    }
                }
            }
            if (count == 0) count = 1;
            new_pixels[y][x].R = sumR / count;
            new_pixels[y][x].G = sumG / count;
            new_pixels[y][x].B = sumB / count;
        }
    }

    // free old image
    for (int i = 0; i < image.height; i++) {
        delete[] image.pixels[i];
    }
    delete[] image.pixels;

    // update image
    image.pixels = new_pixels;
    image.width = new_width;
    image.height = new_height;
}