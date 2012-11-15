#pragma once

#include <zmq.hpp>
#include <string>
#include <server/message_factory.hpp>
#include <server/state.hpp>
#include <model/tetromino.hpp>

class ServerInterface {
public:
    ServerInterface(zmq::context_t& context, 
                    std::string const& commandServer,
                    std::string const& stateServer,
                    std::string const& matchToken);
    ~ServerInterface();
    // Performs initial handshake with server
    bool Initialize();
    // Polls state
    State const* GetState();
    // Sends move
    bool SendMove(enum Tetromino::Move move, int pieceId);
private:
    zmq::socket_t m_command;
    zmq::socket_t m_state;
    std::string const& m_matchToken;
    MessageFactory m_factory;
};

