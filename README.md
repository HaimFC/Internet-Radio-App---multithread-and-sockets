# Internet Radio Application

## Overview
This project involves designing and implementing an Internet Radio application that enables multiple clients to connect to a server to stream songs. The system allows users to:
- Listen to streaming songs.
- Change between radio stations.
- Upload new songs to the server.

The implementation focuses on:
- TCP for control data (e.g., commands, metadata).
- UDP multicast for streaming song data.

---

## Key Features

1. **Multithreaded Server:**
   - Handles multiple clients simultaneously.
   - Streams songs using UDP multicast.
   - Allows new song uploads to create additional stations.

2. **Client Application:**
   - Plays songs streamed by the server.
   - Sends commands to the server (e.g., change station, upload songs).
   - Receives and processes server responses.

3. **Protocol Design:**
   - Control messages via TCP.
   - Song streaming via UDP.

---

## Server States
The server operates in various states as defined in the `radio_server.c` file:
1. **Default State**: Initializes sockets and waits for client connections.
2. **Established State**: Manages active clients and streams songs.
3. **Upload State**: Handles song uploads from clients, creating new stations for each successful upload.
![Alt Text](path/to/image.png)
---

## Client States
The client application transitions through different states as defined in `radio_controller.c`:
1. **Default State**: Initializes and connects to the server.
2. **Welcome Waiting State**: Awaits a "Welcome" message from the server.
3. **Established State**: Streams songs and processes user commands.
4. **Upload Waiting State**: Handles song uploads.
![Alt Text](path/to/image.png)
---

## State Diagrams

### Server State Diagram
![Server State Diagram](sandbox:/mnt/data/image.png)

### Client State Diagram
![Client State Diagram](sandbox:/mnt/data/image.png)

---

## Protocol Summary

### Messages
1. **Client-to-Server Commands:**
   - `Hello`: Initializes connection.
   - `AskSong`: Requests song details from a station.
   - `UpSong`: Initiates a song upload.

2. **Server-to-Client Replies:**
   - `Welcome`: Sent after a successful connection.
   - `Announce`: Provides details about the requested song.
   - `PermitSong`: Indicates whether a song upload is allowed.
   - `InvalidCommand`: Indicates protocol violations.

---

## Implementation Highlights

### Server (`radio_server.c`)
1. **Song Streaming via UDP:**
   - Utilizes multicast groups for each station.
   - Streams songs at a fixed rate.

2. **Client Management:**
   - Maintains active client connections.
   - Handles song upload requests.

3. **Multithreading:**
   - Separate threads for each client and station.

---

### Client (`radio_controller.c`)
1. **User Commands:**
   - Switch stations by entering station numbers.
   - Upload songs using the `s` command.
   - Quit the application with the `q` command.

2. **Song Playback:**
   - Receives and plays songs streamed via UDP.

3. **Error Handling:**
   - Handles protocol violations gracefully by disconnecting.

---

## Summary
The Internet Radio Application demonstrates an effective use of network programming techniques, including TCP/UDP communication, multithreading, and real-time streaming. The application provides a robust platform for streaming songs, managing multiple clients, and dynamically creating new stations.
