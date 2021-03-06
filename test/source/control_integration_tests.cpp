#include <test/control_integration_tests.hpp>
#include <core/game_state_eval.hpp>
#include <core/tetris_oracle.hpp>
#include <util/constants.hpp>

CPPUNIT_TEST_SUITE_REGISTRATION(ControlIntegrationTests);

TestServerInterface::TestServerInterface()
    : m_messages(), m_commandConnected(false), m_stateConnected(false)
{ 
    pthread_mutex_init(&m_messageMutex, NULL);
    pthread_mutex_init(&m_moveMutex, NULL);
    pthread_cond_init(&m_messagesAvailable, NULL);
    pthread_cond_init(&m_movesAvailable, NULL);
}

TestServerInterface::~TestServerInterface()
{ 
    pthread_mutex_destroy(&m_messageMutex);
    pthread_mutex_destroy(&m_moveMutex);
    pthread_cond_destroy(&m_messagesAvailable);
    pthread_cond_destroy(&m_movesAvailable);
    while (!m_messages.empty()) {
        State const* s = m_messages.front();
        m_messages.pop();
        if (s != NULL) {
            delete s;
            s = NULL;
        }
    }
}

bool TestServerInterface::ConnectToCommandServer(zmq::socket_t&) const {
    m_commandConnected = true;
    return m_commandConnected;
}

void TestServerInterface::ConnectToStateServer(zmq::socket_t&) const {
    m_stateConnected = true;
}

State const* TestServerInterface::GetState(zmq::socket_t&) const {
    if (!m_stateConnected)
        return NULL;
    pthread_mutex_lock(&m_messageMutex);
    // Waits for messages to be available
    if (m_messages.empty()) {
        pthread_cond_wait(&m_messagesAvailable, &m_messageMutex);
    }
    State const* message = m_messages.front();
    m_messages.pop();
    pthread_mutex_unlock(&m_messageMutex);
    return message;
}

bool TestServerInterface::SendMove(enum Tetromino::Move move, int pieceId, 
                                    zmq::socket_t&) const {
    if (!m_commandConnected)
        return false;
    pthread_mutex_lock(&m_moveMutex);
    m_moves.push(std::pair<enum Tetromino::Move, int>(move, pieceId));
    pthread_cond_signal(&m_movesAvailable);
    pthread_mutex_unlock(&m_moveMutex);
    return true;
}

void TestServerInterface::AddStateMessage(State const* message) const {
    pthread_mutex_lock(&m_messageMutex);
    m_messages.push(message);
    pthread_cond_signal(&m_messagesAvailable);
    pthread_mutex_unlock(&m_messageMutex);
}

bool TestServerInterface::HasMoveMessages() const {
    bool haveMessages = false;
    pthread_mutex_lock(&m_moveMutex);
    haveMessages = !m_moves.empty();
    pthread_mutex_unlock(&m_moveMutex);
    return haveMessages;
}

std::pair<enum Tetromino::Move, int> TestServerInterface::GetNextMove() const {
    pthread_mutex_lock(&m_moveMutex);
    if (m_moves.empty()) {
        pthread_cond_wait(&m_movesAvailable, &m_moveMutex);
    }
    std::pair<enum Tetromino::Move, int> move = m_moves.front();
    m_moves.pop();
    pthread_mutex_unlock(&m_moveMutex);
    return move;
}

InterceptInternalState::InterceptInternalState(GameState*& gameStatePointer,
                                                pthread_mutex_t& pointerMutex,
                                                pthread_cond_t& pointerSet)
    : m_gameStatePointer(gameStatePointer), m_pointerMutex(pointerMutex),
        m_pointerSet(pointerSet)
{ }

bool InterceptInternalState::ExecuteUpdates(GameState& game) const {
    pthread_mutex_lock(&m_pointerMutex);
    m_gameStatePointer = &game;
    pthread_cond_signal(&m_pointerSet);
    pthread_mutex_unlock(&m_pointerMutex);
    return false;
}

bool InterceptInternalState::ShouldTerminate() const {
    return false;
}

SynchronizeMessage::SynchronizeMessage(pthread_mutex_t& signalMutex,
                                        pthread_cond_t& signal) 
    : m_signalMutex(signalMutex), m_signal(signal)
{ }

bool SynchronizeMessage::ExecuteUpdates(GameState&) const {
    pthread_mutex_lock(&m_signalMutex);
    pthread_cond_signal(&m_signal);
    pthread_mutex_unlock(&m_signalMutex);
    return false;
}

bool SynchronizeMessage::ShouldTerminate() const {
    return false;
}

bool TerminateExecution::ExecuteUpdates(GameState&) const {
    return true;
}

bool TerminateExecution::ShouldTerminate() const {
    return true;
}

void* ControlExecute(void* varg) {
    Control* c = static_cast<Control*>(varg);
    c->Execute();
    return varg;
}

Tetromino const* ControlIntegrationTests::TestApplyPath(GameState& game, 
                        std::vector< std::pair<enum Tetromino::Move, int> > const& path) const {
    GameBoard const& board = game.GetBoard();
    Tetromino const* inPlay = game.GetPieceInPlay();
    if (inPlay == NULL) {
        return NULL;
    }
    Tetromino t = *inPlay;
    if (!board.IsValidMove(t)) {
        return NULL;
    }
    for (unsigned i = 0; i < path.size(); ++i) {
        enum Tetromino::Move move = path.at(i).first; 
        if (move == Tetromino::left)
            t.ShiftLeft();
        else if (move == Tetromino::right)
            t.ShiftRight();
        else if (move == Tetromino::down)
            t.ShiftDown();
        else if (move == Tetromino::rrotate)
            t.RotateRight();
        else if (move == Tetromino::lrotate)
            t.RotateLeft();
        else if (move == Tetromino::drop) {
            while (board.IsValidMove(t) && !board.IsAtRest(t)) {
                t.ShiftDown(); 
            }
        }
        if (!board.IsValidMove(t)) {
            return NULL;
        }
    }
    if (!board.IsAtRest(t)) {
        return NULL;
    }
    game.ApplyMove(t);
    return new Tetromino(t);
}

void ControlIntegrationTests::TestPlacePiece() {
    TestServerInterface si;
    GameState* internalGame = NULL;
    GameState serverGame;
    pthread_mutex_t pointerMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t pointerSet = PTHREAD_COND_INITIALIZER;
    State const* intercept = new InterceptInternalState(internalGame, 
                                                        pointerMutex,
                                                        pointerSet);
    char* boardDesc = new char[BOARD_DESC_SIZE];
    for (unsigned i = 0; i < BOARD_DESC_SIZE; ++i) {
        boardDesc[i] = '0';
    }
    State const* boardMessage = new GameBoardState(0, 1.0, boardDesc, 0,
                                                    new std::vector<int>());
    Tetromino* tet = new Tetromino('S', 0, 5, 1);
    State const* pieceMessage = new GamePieceState(1, 2.0, tet, 0,
                                                new std::vector<Tetromino>());
    si.AddStateMessage(intercept);
    si.AddStateMessage(boardMessage);
    si.AddStateMessage(pieceMessage);
    Harmony const* weights = GetBestHarmony();
    zmq::context_t context (1);
    Control command (context, si, *weights);

    serverGame.SetPieceInPlay(tet);
    
    // Launches main execute loop, waits to intercept state
    pthread_mutex_lock(&pointerMutex);
    pthread_t execute = 0;
    pthread_create(&execute, NULL, ControlExecute, static_cast<void*>(&command));
    pthread_cond_wait(&pointerSet, &pointerMutex);
    pthread_mutex_unlock(&pointerMutex);

    // Waits for/gets first move
    std::vector< std::pair<enum Tetromino::Move, int> > firstMove;
    std::pair<enum Tetromino::Move, int> move = si.GetNextMove();
    firstMove.push_back(move);

    // Synchronizes with execute loop
    pthread_mutex_t syncMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t syncWithControl = PTHREAD_COND_INITIALIZER;
    State const* sync = new SynchronizeMessage(syncMutex, syncWithControl);
    pthread_mutex_lock(&syncMutex);
    si.AddStateMessage(sync);
    pthread_cond_wait(&syncWithControl, &syncMutex);
    pthread_mutex_unlock(&syncMutex);

    // Grab move messages
    while (si.HasMoveMessages()) {
        move = si.GetNextMove();
        firstMove.push_back(move);
    }

    // Record first move results
    Tetromino const* firstExpected = FindBestMove(serverGame, *weights);
    Tetromino const* firstActual = TestApplyPath(serverGame, firstMove);

    // Second move
    std::vector< std::pair<enum Tetromino::Move, int> > secondMove;
    Tetromino const* secondExpected = NULL;
    Tetromino const* secondActual = NULL;
    if (firstExpected != NULL && firstActual != NULL) {
        GameBoard const& serverBoard = serverGame.GetBoard();
        char const* newBoardDesc = serverBoard.GenerateDesc();
        boardMessage = new GameBoardState(3, 3.0, newBoardDesc, 2,
                                            new std::vector<int>());
        tet = new Tetromino('I', 0, 5, 1);
        serverGame.SetPieceInPlay(tet);
        pieceMessage = new GamePieceState(4, 4.0, tet, 2,
                                        new std::vector<Tetromino>());
        si.AddStateMessage(boardMessage);
        si.AddStateMessage(pieceMessage);

        move = si.GetNextMove();
        secondMove.push_back(move);

        // Synchronizes
        sync = new SynchronizeMessage(syncMutex, syncWithControl);
        pthread_mutex_lock(&syncMutex);
        si.AddStateMessage(sync);
        pthread_cond_wait(&syncWithControl, &syncMutex);
        pthread_mutex_unlock(&syncMutex);
        
        // Grab move messages
        while (si.HasMoveMessages()) {
            move = si.GetNextMove();
            secondMove.push_back(move);
        }

        // Record second move results
        secondExpected = FindBestMove(serverGame, *weights);
        secondActual = TestApplyPath(serverGame, secondMove);
    }

    // cleanup
    State const* term = new TerminateExecution();
    si.AddStateMessage(term);
    pthread_join(execute, NULL);
    pthread_mutex_destroy(&pointerMutex);
    pthread_cond_destroy(&pointerSet);
    pthread_mutex_destroy(&syncMutex);
    pthread_cond_destroy(&syncWithControl);
    delete weights;

    CPPUNIT_ASSERT(firstMove.size() > 0);
    CPPUNIT_ASSERT(secondMove.size() > 0);
    CPPUNIT_ASSERT(firstExpected != NULL);
    CPPUNIT_ASSERT(firstActual != NULL);
    CPPUNIT_ASSERT(*firstExpected == *firstActual);
    CPPUNIT_ASSERT(secondExpected != NULL);
    CPPUNIT_ASSERT(secondActual != NULL);
    CPPUNIT_ASSERT(*secondExpected == *secondActual);
    for (unsigned i = 0; i < firstMove.size(); ++i) {
        CPPUNIT_ASSERT_EQUAL(0, firstMove.at(i).second);
    }
    for (unsigned i = 0; i < secondMove.size(); ++i) {
        CPPUNIT_ASSERT_EQUAL(2, secondMove.at(i).second);
    }

    delete firstExpected;
    delete firstActual;
    delete secondExpected;
    delete secondActual;
}

