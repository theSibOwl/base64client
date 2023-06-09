#include <iostream>
#include <string>
#include <bitset>
#include <algorithm>
#include <locale>
#include <cstring> // For memset

#ifdef _WIN32 // Compilation for Windows
#include "getopt_windows.h" // Alternative implementation getopt() for Windows
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else // Compilation for UNIX
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

static const std::string base64_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string StringToBase64(const std::string& source_string) {
    // Converting a source string to a bit sequence
    std::string bit_string;
    std::bitset<8> binary_char;
    for (const char& c : source_string) {
        binary_char = std::bitset<8>(c);
        bit_string += binary_char.to_string();
    }
    // Finding the remainder of the division, then adding zero bits for alignment
    size_t remainder = bit_string.size() % 24;
    if (remainder == 8) {
        bit_string.append(4, '0');
    }
    else if (remainder == 16) {
        bit_string.append(2, '0');
    }
    // Converting a bit sequence to base64
    std::string base64_string;
    std::bitset<6> binary_number;
    for (size_t i = 0; i < bit_string.size(); i += 6) {
        binary_number = std::bitset<6>(bit_string.substr(i, 6));
        base64_string += base64_alphabet[binary_number.to_ulong()];
    }
    // Filling with "=" characters if the source data is not completely divided into 3 bytes
    if (remainder == 8) {
        base64_string.append(2, '=');
    }
    else if (remainder == 16) {
        base64_string.append(1, '=');
    }

    return base64_string;
}

std::string Base64ToString(const std::string& base64_string) {
    // Finding the number of occurrences of the '=' character to skip it
    int counter = std::count(base64_string.begin(), base64_string.end(), '=');
    // Converting a base64 string to a bit sequence
    std::string bit_string;
    std::bitset<6> binary_number;
    for (size_t i = 0; i < base64_string.size() - counter; i++) {
        binary_number = std::bitset<6>(base64_alphabet.find(base64_string[i]));
        bit_string += binary_number.to_string();
    }
    // Removing extra zero bits
    if (counter == 2) {
        bit_string.resize(bit_string.size() - 4);
    }
    else if (counter == 1) {
        bit_string.resize(bit_string.size() - 2);
    }
    // Converting a bit sequence to a regular string
    std::string decoded_string;
    std::bitset<8> binary_char;
    for (size_t i = 0; i < bit_string.size(); i += 8) {
        binary_char = std::bitset<8>(bit_string.substr(i, 8));
        decoded_string += static_cast<char>(binary_char.to_ulong());
    }

    return decoded_string;
}

void PrintHelp() {
    std::cout << "Usage: base64client [OPTIONS] [TEXT]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -e, --encode    Encode the following text to base64 and send it to the server." << std::endl;
    std::cout << "                  Requires -s/--server and -p/--port options." << std::endl;
    std::cout << "  -d, --decode    Decode the following base64 text to text." << std::endl;
    std::cout << "  -s, --server    Set the server address (required for -e/--encode)." << std::endl;
    std::cout << "  -p, --port      Set the server port number (required for -e/--encode)." << std::endl;
    std::cout << "  -?, -h, --help  Display this help message." << std::endl;
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "");

    std::string server_ip;
    std::string server_port;
    std::string input_text;
    bool encode_mode = false;
    bool decode_mode = false;

    const option long_options[] = {
        {"encode", required_argument, nullptr, 'e'},
        {"decode", required_argument, nullptr, 'd'},
        {"help", no_argument, nullptr, 'h'},
        {"server", required_argument, nullptr, 's'},
        {"port", required_argument, nullptr, 'p'},
        {nullptr, 0, nullptr, 0}
    };

    // Parsing arguments using 'getopt' function
    int opt;
    while ((opt = getopt_long(argc, argv, "e:d:hs:p:", long_options, nullptr)) != -1) {
        switch (opt) {
        case 'e':
            encode_mode = true;
            input_text = optarg;
            break;
        case 'd':
            decode_mode = true;
            input_text = optarg;
            break;
        case 's':
            server_ip = optarg;
            break;
        case 'p':
            server_port = optarg;
            break;
        case 'h':
            PrintHelp();
            return 0;
        default:
            std::cerr << "Unknown option: ";
            if (optopt) {
                std::cerr << "-" << static_cast<char>(optopt);
            }
            else {
                std::cerr << argv[optind - 1];
            }
            std::cerr << std::endl;
            PrintHelp();
            return 1;
        }
    }

    // Text processing
    if (optind < argc) {
        input_text = argv[optind];
    }

    // Encoding or decoding, depending on the selected mode
    if (encode_mode && decode_mode) {
        std::cerr << "Error: Cannot use both encode and decode options." << std::endl;
        return 1;
    }
    else if (encode_mode) {
        // Checking if all required arguments are provided
        if (server_ip.empty() || server_port.empty() || input_text.empty()) {
            PrintHelp();
            return 1;
        }

#ifdef _WIN32 // Initialization for Windows
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Failed to initialize winsock" << std::endl;
            return 1;
        }
#endif

        // Creating a socket
        int client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            std::cerr << "Failed to create socket" << std::endl;
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }

        // Setting up the server details
        sockaddr_in server_address{};
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(std::stoi(server_port));
        if (inet_pton(AF_INET, server_ip.c_str(), &(server_address.sin_addr)) <= 0) {
            std::cerr << "Invalid address or address not supported" << std::endl;
#ifdef _WIN32
            closesocket(client_socket);
            WSACleanup();
#else
            close(client_socket);
#endif
            return 1;
        }

        // Connecting to the server
        if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) < 0) {
            std::cerr << "Failed to connect to the server" << std::endl;
#ifdef _WIN32
            closesocket(client_socket);
            WSACleanup();
#else
            close(client_socket);
#endif
            return 1;
        }

        // Encoding mode activated
        std::cout << "Encoding mode activated." << std::endl;

        // Sending the string to the server
        std::string encoded_string = StringToBase64(input_text);
        if (send(client_socket, encoded_string.c_str(), encoded_string.size(), 0) < 0) {
            std::cerr << "Failed to send data to the server" << std::endl;
#ifdef _WIN32
            closesocket(client_socket);
            WSACleanup();
#else
            close(client_socket);
#endif
            return 1;
        }

#ifdef _WIN32
        Sleep(2000); // Delay for 2 seconds
#else
        sleep(2); // Delay for 2 seconds
#endif

        // Closing the connection
#ifdef _WIN32
        closesocket(client_socket);
        WSACleanup();
#else
        close(client_socket);
#endif
    }
    else if (decode_mode) {
        std::string decoded_text = Base64ToString(input_text);
        std::cout << decoded_text << std::endl;
    }
    else {
        std::cerr << "No mode selected. Use -e for encoding or -d for decoding." << std::endl;
        return 1;
    }

    return 0;
}