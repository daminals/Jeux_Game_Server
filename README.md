![github repo badge: Language](https://img.shields.io/badge/Language-C-181717?color=blue) ![github repo badge: OS](https://img.shields.io/badge/OS-Linux-181717?color=yellow)

# Jeux_Game_Server
A multithreaded game server designed to play TicTacToe via remote connection

## Introduction

The Jeux Game Server project is a multiplayer game server implementation that allows users to play a variety of two-player games. The server provides a platform for players to connect, compete, and interact with each other in a gaming environment. The focus of this project is to develop a robust and scalable server architecture capable of handling multiple concurrent game sessions while ensuring fair gameplay and synchronization between players.

## Features

- Support for two-player games: The server supports two-player games, starting with tic-tac-toe as the default game. The design allows for easy integration of additional games.
User registration and authentication: Players can create user accounts, log in, and authenticate their identities before accessing the game server.
- Real-time gameplay: Players can engage in real-time gameplay sessions, taking turns and interacting with each other.
- Ratings and rankings: The server assigns numerical ratings to players based on their performance in games, allowing for the creation of rankings and leaderboards.
- Multi-threading and concurrency: The server utilizes POSIX threads to handle multiple game sessions concurrently, ensuring efficient resource utilization and responsiveness.
- Networking: The server uses socket programming to establish network connections with clients and facilitate data exchange.

## How it Works

The Jeux Game Server operates based on the following key components and functionalities:

1. Server Initialization: Upon starting the server, it initializes the necessary resources, such as network sockets, data structures, and thread pools, to handle incoming client connections and game sessions.
2. User Registration and Authentication: Players can create user accounts by providing a username and password. The server securely stores user credentials and performs authentication during login to ensure the integrity of user identities.
3. Game Sessions: Once players are connected, the server creates a game session for them. The game session manages the game state, enforces game rules, and facilitates turn-based gameplay between the players.
4. Concurrency and Synchronization: The server employs multi-threading techniques, using POSIX threads, to handle multiple game sessions concurrently. Thread synchronization mechanisms such as mutexes and semaphores are utilized to ensure data integrity and prevent race conditions.
5. Ratings and Rankings: The server tracks players' performance in games and calculates numerical ratings based on their wins, losses, and other factors. These ratings can be used to create rankings and leaderboards, providing a competitive environment for players.
6. Networking: The server utilizes socket programming to establish network connections with clients. It handles incoming client requests, processes game-related data, and sends updates and notifications to connected players in real-time.
7. Error Handling and Logging: The server incorporates error handling mechanisms to handle exceptions, recover from failures, and provide informative error messages to clients. It also includes logging functionality to record server activities and debugging information for troubleshooting purposes.

## Usage

1. Players should first register user accounts on the server by providing a username and password.
2. After registration, players can log in using their credentials to access the game server.
3. Once logged in, players can request to join a game



