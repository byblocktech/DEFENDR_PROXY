#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <unordered_set>
#include <memory>
#include <fstream>
#include <array>

using boost::asio::ip::tcp;

// Extracting the name host
std::string get_host(const std::string& request) {
    // Busca por "Host:" ou "host:" and pointer to first char
    size_t pos = request.find("Host:");
    //npos not position func, to cases without find
    if (pos == std::string::npos) pos = request.find("host:");
    if (pos == std::string::npos) return "";

    // pointer to first += 5 to go to first char of url ->H(0)->o(1)->s(2)->t(3)->:(4)
    pos += 5;

    while (pos < request.size() && (request[pos] == ' ' || request[pos] == '\t')) {
        pos++;
    }
    //baseboard of tcp protol(http) \r\n
    size_t end = request.find("\r\n", pos);
    if (end == std::string::npos) return "";

    return request.substr(pos, end - pos);
}

class Session : public std::enable_shared_from_this<Session> {
public:

    Session(tcp::socket socket, const std::unordered_set<std::string>& blocklist)
    : client_socket_(std::move(socket)), blocklist_(blocklist) {}

    void start() { do_read(); }

private:
    //first inssue in public, but maybe not necessary
    void do_read() {
        //this to control the pointer obj and self to control the time life obj
        auto self = shared_from_this();
        client_socket_.async_read_some(boost::asio::buffer(data_),
                                       [this, self](boost::system::error_code ec, std::size_t length) {
                                           if (!ec) {
                                               std::string request(data_.data(), length);
                                               std::string host = get_host(request);

                                               if (!host.empty() && blocklist_.count(host)) {
                                                   std::cout << "[BLOQUEADO] " << host << std::endl;
                                                   client_socket_.close();
                                                   return;
                                               }

                                               if (!host.empty()) std::cout << "[PERMITIDO] " << host << std::endl;

                                               // Continua lendo (loop assíncrono)
                                               do_read();
                                           }
                                       });
    }

    tcp::socket client_socket_;
    std::array<char, 8192> data_; // Buffer was 1024, but headers HTTP maybe need be larger
    const std::unordered_set<std::string>& blocklist_; //take of .txt and put it on memory, ok, more memory request but more efficient too. calculate 150kb of ram to it. can be wrong, study about
};

class ProxyServer {
public:
    ProxyServer(boost::asio::io_context& context, short port)
    : acceptor_(context, tcp::endpoint(tcp::v4(), port)) {

        load_block_list();
        start_accept();
    }

private:
    void load_block_list() {
        blocklist_.reserve(1500000);
        std::ifstream file("blocklist.txt");
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) blocklist_.insert(line);
        }
        std::cout << "Blocklist carregada: " << blocklist_.size() << " domínios." << std::endl;
    }

    void start_accept() {
        // Aceita a conexão e já cria a Session automaticamente
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), blocklist_)->start();
                }
                start_accept(); // Volta a esperar o próximo cliente imediatamente
            });
    }

    tcp::acceptor acceptor_;
    std::unordered_set<std::string> blocklist_;
};

int main() {
    try {
        boost::asio::io_context io_context;
        ProxyServer server(io_context, 12345);
        io_context.run(); // O loop de eventos roda aqui
    } catch (std::exception& e) {
        std::cerr << "Exceção: " << e.what() << std::endl;
    }
    return 0;
}
