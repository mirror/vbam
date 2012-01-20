////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2009 Laurent Gomila (laurent.gom@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Network/SocketTCP.hpp>
#include <SFML/Network/IPAddress.hpp>
#include <SFML/Network/Packet.hpp>
#include <SFML/Network/SocketHelper.hpp>
#include <algorithm>
#include <iostream>
#include <string.h>


#ifdef _MSC_VER
    #pragma warning(disable : 4127) // "conditional expression is constant" generated by the FD_SET macro
#endif


namespace sf
{
////////////////////////////////////////////////////////////
/// Default constructor
////////////////////////////////////////////////////////////
SocketTCP::SocketTCP()
{
    Create(SocketHelper::InvalidSocket());
}


////////////////////////////////////////////////////////////
/// Change the blocking state of the socket
////////////////////////////////////////////////////////////
void SocketTCP::SetBlocking(bool Blocking)
{
    // Make sure our socket is valid
    if (!IsValid())
        Create();

    SocketHelper::SetBlocking(mySocket, Blocking);
    myIsBlocking = Blocking;
}


////////////////////////////////////////////////////////////
/// Connect to another computer on a specified port
////////////////////////////////////////////////////////////
Socket::Status SocketTCP::Connect(unsigned short Port, const IPAddress& HostAddress, float Timeout)
{
    // Make sure our socket is valid
    if (!IsValid())
        Create();

    // Build the host address
    sockaddr_in SockAddr;
    memset(SockAddr.sin_zero, 0, sizeof(SockAddr.sin_zero));
    SockAddr.sin_addr.s_addr = inet_addr(HostAddress.ToString().c_str());
    SockAddr.sin_family      = AF_INET;
    SockAddr.sin_port        = htons(Port);

    if (Timeout <= 0)
    {
        // ----- We're not using a timeout : just try to connect -----

        if (connect(mySocket, reinterpret_cast<sockaddr*>(&SockAddr), sizeof(SockAddr)) == -1)
        {
            // Failed to connect
            return SocketHelper::GetErrorStatus();
        }

        // Connection succeeded
        return Socket::Done;
    }
    else
    {
        // ----- We're using a timeout : we'll need a few tricks to make it work -----

        // Save the previous blocking state
        bool IsBlocking = myIsBlocking;

        // Switch to non-blocking to enable our connection timeout
        if (IsBlocking)
            SetBlocking(false);

        // Try to connect to host
        if (connect(mySocket, reinterpret_cast<sockaddr*>(&SockAddr), sizeof(SockAddr)) >= 0)
        {
            // We got instantly connected! (it may no happen a lot...)
            return Socket::Done;
        }

        // Get the error status
        Socket::Status Status = SocketHelper::GetErrorStatus();

        // If we were in non-blocking mode, return immediatly
        if (!IsBlocking)
            return Status;

        // Otherwise, wait until something happens to our socket (success, timeout or error)
        if (Status == Socket::NotReady)
        {
            // Setup the selector
            fd_set Selector;
            FD_ZERO(&Selector);
            FD_SET(mySocket, &Selector);

            // Setup the timeout
            timeval Time;
            Time.tv_sec  = static_cast<long>(Timeout);
            Time.tv_usec = (static_cast<long>(Timeout * 1000) % 1000) * 1000;

            // Wait for something to write on our socket (which means that the connection request has returned)
            if (select(static_cast<int>(mySocket + 1), NULL, &Selector, NULL, &Time) > 0)
            {
                // At this point the connection may have been either accepted or refused.
                // To know whether it's a success or a failure, we try to retrieve the name of the connected peer
                SocketHelper::LengthType Size = sizeof(SockAddr);
                if (getpeername(mySocket, reinterpret_cast<sockaddr*>(&SockAddr), &Size) != -1)
                {
                    // Connection accepted
                    Status = Socket::Done;
                }
                else
                {
                    // Connection failed
                    Status = SocketHelper::GetErrorStatus();
                }
            }
            else
            {
                // Failed to connect before timeout is over
                Status = SocketHelper::GetErrorStatus();
            }
        }

        // Switch back to blocking mode
        SetBlocking(true);

        return Status;
    }
}


////////////////////////////////////////////////////////////
/// Listen to a specified port for incoming data or connections
////////////////////////////////////////////////////////////
bool SocketTCP::Listen(unsigned short Port)
{
    // Make sure our socket is valid
    if (!IsValid())
        Create();

    // Build the address
    sockaddr_in SockAddr;
    memset(SockAddr.sin_zero, 0, sizeof(SockAddr.sin_zero));
    SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    SockAddr.sin_family      = AF_INET;
    SockAddr.sin_port        = htons(Port);

    // Bind the socket to the specified port
    if (bind(mySocket, reinterpret_cast<sockaddr*>(&SockAddr), sizeof(SockAddr)) == -1)
    {
        // Not likely to happen, but...
        std::cerr << "Failed to bind socket to port " << Port << std::endl;
        return false;
    }

    // Listen to the bound port
    if (listen(mySocket, 0) == -1)
    {
        // Oops, socket is deaf
        std::cerr << "Failed to listen to port " << Port << std::endl;
        return false;
    }

    return true;
}


////////////////////////////////////////////////////////////
/// Wait for a connection (must be listening to a port).
/// This function will block if the socket is blocking
////////////////////////////////////////////////////////////
Socket::Status SocketTCP::Accept(SocketTCP& Connected, IPAddress* Address)
{
    // Address that will be filled with client informations
    sockaddr_in ClientAddress;
    SocketHelper::LengthType Length = sizeof(ClientAddress);

    // Accept a new connection
    Connected = accept(mySocket, reinterpret_cast<sockaddr*>(&ClientAddress), &Length);

    // Check errors
    if (!Connected.IsValid())
    {
        if (Address)
            *Address = IPAddress();

        return SocketHelper::GetErrorStatus();
    }

    // Fill address if requested
    if (Address)
        *Address = IPAddress(inet_ntoa(ClientAddress.sin_addr));

    return Socket::Done;
}


////////////////////////////////////////////////////////////
/// Send an array of bytes to the host (must be connected first)
////////////////////////////////////////////////////////////
Socket::Status SocketTCP::Send(const char* Data, std::size_t Size)
{
    // First check that socket is valid
    if (!IsValid())
        return Socket::Error;

    // Check parameters
    if (Data && Size)
    {
        // Loop until every byte has been sent
        int Sent = 0;
        int SizeToSend = static_cast<int>(Size);
        for (int Length = 0; Length < SizeToSend; Length += Sent)
        {
            // Send a chunk of data
            Sent = send(mySocket, Data + Length, SizeToSend - Length, 0);

            // Check if an error occured
            if (Sent <= 0)
                return SocketHelper::GetErrorStatus();
        }

        return Socket::Done;
    }
    else
    {
        // Error...
        std::cerr << "Cannot send data over the network (invalid parameters)" << std::endl;
        return Socket::Error;
    }
}


////////////////////////////////////////////////////////////
/// Receive an array of bytes from the host (must be connected first).
/// This function will block if the socket is blocking
////////////////////////////////////////////////////////////
Socket::Status SocketTCP::Receive(char* Data, std::size_t MaxSize, std::size_t& SizeReceived)
{
    // First clear the size received
    SizeReceived = 0;

    // Check that socket is valid
    if (!IsValid())
        return Socket::Error;

    // Check parameters
    if (Data && MaxSize)
    {
        // Receive a chunk of bytes
        int Received = recv(mySocket, Data, static_cast<int>(MaxSize), 0);

        // Check the number of bytes received
        if (Received > 0)
        {
            SizeReceived = static_cast<std::size_t>(Received);
            return Socket::Done;
        }
        else if (Received == 0)
        {
            return Socket::Disconnected;
        }
        else
        {
            return SocketHelper::GetErrorStatus();
        }
    }
    else
    {
        // Error...
        std::cerr << "Cannot receive data from the network (invalid parameters)" << std::endl;
        return Socket::Error;
    }
}


////////////////////////////////////////////////////////////
/// Send a packet of data to the host (must be connected first)
////////////////////////////////////////////////////////////
Socket::Status SocketTCP::Send(Packet& PacketToSend)
{
    // Get the data to send from the packet
    std::size_t DataSize = 0;
    const char* Data = PacketToSend.OnSend(DataSize);

    // Send the packet size
    Uint32 PacketSize = htonl(static_cast<unsigned long>(DataSize));
    Send(reinterpret_cast<const char*>(&PacketSize), sizeof(PacketSize));

    // Send the packet data
    if (PacketSize > 0)
    {
        return Send(Data, DataSize);
    }
    else
    {
        return Socket::Done;
    }
}


////////////////////////////////////////////////////////////
/// Receive a packet from the host (must be connected first).
/// This function will block if the socket is blocking
////////////////////////////////////////////////////////////
Socket::Status SocketTCP::Receive(Packet& PacketToReceive)
{
    // We start by getting the size of the incoming packet
    Uint32      PacketSize = 0;
    std::size_t Received   = 0;
    if (myPendingPacketSize < 0)
    {
        // Loop until we've received the entire size of the packet
        // (even a 4 bytes variable may be received in more than one call)
        while (myPendingHeaderSize < sizeof(myPendingHeader))
        {
            char* Data = reinterpret_cast<char*>(&myPendingHeader) + myPendingHeaderSize;
            Socket::Status Status = Receive(Data, sizeof(myPendingHeader) - myPendingHeaderSize, Received);
            myPendingHeaderSize += Received;

            if (Status != Socket::Done)
                return Status;
        }

        PacketSize = ntohl(myPendingHeader);
        myPendingHeaderSize = 0;
    }
    else
    {
        // There is a pending packet : we already know its size
        PacketSize = myPendingPacketSize;
    }

    // Then loop until we receive all the packet data
    char Buffer[1024];
    while (myPendingPacket.size() < PacketSize)
    {
        // Receive a chunk of data
        std::size_t SizeToGet = std::min(static_cast<std::size_t>(PacketSize - myPendingPacket.size()), sizeof(Buffer));
        Socket::Status Status = Receive(Buffer, SizeToGet, Received);
        if (Status != Socket::Done)
        {
            // We must save the size of the pending packet until we can receive its content
            if (Status == Socket::NotReady)
                myPendingPacketSize = PacketSize;
            return Status;
        }

        // Append it into the packet
        if (Received > 0)
        {
            myPendingPacket.resize(myPendingPacket.size() + Received);
            char* Begin = &myPendingPacket[0] + myPendingPacket.size() - Received;
            memcpy(Begin, Buffer, Received);
        }
    }

    // We have received all the datas : we can copy it to the user packet, and clear our internal packet
    PacketToReceive.Clear();
    if (!myPendingPacket.empty())
        PacketToReceive.OnReceive(&myPendingPacket[0], myPendingPacket.size());
    myPendingPacket.clear();
    myPendingPacketSize = -1;

    return Socket::Done;
}


////////////////////////////////////////////////////////////
/// Close the socket
////////////////////////////////////////////////////////////
bool SocketTCP::Close()
{
    if (IsValid())
    {
        if (!SocketHelper::Close(mySocket))
        {
            std::cerr << "Failed to close socket" << std::endl;
            return false;
        }

        mySocket = SocketHelper::InvalidSocket();
    }

    myIsBlocking = true;

    return true;
}


////////////////////////////////////////////////////////////
/// Check if the socket is in a valid state ; this function
/// can be called any time to check if the socket is OK
////////////////////////////////////////////////////////////
bool SocketTCP::IsValid() const
{
    return mySocket != SocketHelper::InvalidSocket();
}


////////////////////////////////////////////////////////////
/// Comparison operator ==
////////////////////////////////////////////////////////////
bool SocketTCP::operator ==(const SocketTCP& Other) const
{
    return mySocket == Other.mySocket;
}


////////////////////////////////////////////////////////////
/// Comparison operator !=
////////////////////////////////////////////////////////////
bool SocketTCP::operator !=(const SocketTCP& Other) const
{
    return mySocket != Other.mySocket;
}


////////////////////////////////////////////////////////////
/// Comparison operator <.
/// Provided for compatibility with standard containers, as
/// comparing two sockets doesn't make much sense...
////////////////////////////////////////////////////////////
bool SocketTCP::operator <(const SocketTCP& Other) const
{
    return mySocket < Other.mySocket;
}


////////////////////////////////////////////////////////////
/// Construct the socket from a socket descriptor
/// (for internal use only)
////////////////////////////////////////////////////////////
SocketTCP::SocketTCP(SocketHelper::SocketType Descriptor)
{
    Create(Descriptor);
}


////////////////////////////////////////////////////////////
/// Create the socket
////////////////////////////////////////////////////////////
void SocketTCP::Create(SocketHelper::SocketType Descriptor)
{
    // Use the given socket descriptor, or get a new one
    mySocket = Descriptor ? Descriptor : socket(PF_INET, SOCK_STREAM, 0);
    myIsBlocking = true;

    // Reset the pending packet
    myPendingHeaderSize = 0;
    myPendingPacket.clear();
    myPendingPacketSize = -1;

    // Setup default options
    if (IsValid())
    {
        // To avoid the "Address already in use" error message when trying to bind to the same port
        int Yes = 1;
        if (setsockopt(mySocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&Yes), sizeof(Yes)) == -1)
        {
            std::cerr << "Failed to set socket option \"SO_REUSEADDR\" ; "
                      << "binding to a same port may fail if too fast" << std::endl;
        }

        // Disable the Nagle algorithm (ie. removes buffering of TCP packets)
        if (setsockopt(mySocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&Yes), sizeof(Yes)) == -1)
        {
            std::cerr << "Failed to set socket option \"TCP_NODELAY\" ; "
                      << "all your TCP packets will be buffered" << std::endl;
        }

        // Set blocking by default (should always be the case anyway)
        SetBlocking(true);
    }
}

} // namespace sf
