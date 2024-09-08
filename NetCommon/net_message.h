#pragma once
#include "net_common.h"

namespace netp
{
    namespace net
    {
        // message header at start of all messages. template allows to ensure messages are valid
        template <typename T>
        struct message_header
        {
            T id{};
            uint32_t size = 0;
        };

        template <typename T>
        struct message
        {
            message_header<T> header{};
            std::vector<uint8_t> body;

            // returns size of message packet in bytes
            size_t size() const
            {
                return sizeof(message_header<T>) + body.size();
            }

            // ovverride for std::cout - produces description of message
            friend std::ostream &operator<<(std::ostream &os, const message<T> &msg)
            {
                os << "ID:" << int(msg.header.id) << " Size:" << msg.header.size;
                return os;
            }

            // pushes any POD-like data into message buffer
            template <typename DataType>
            friend message<T> &operator<<(message<T> &msg, const DataType &data)
            {
                // check compatibiility of data being pushed
                static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed to message buffer");

                // cache current size of vector, as this will be the point we insert the data
                size_t i = msg.body.size();

                // rezsize the vector by the size of data being pushed
                msg.body.resize(msg.body.size() + sizeof(DataType));

                // physically copy data into new vector
                std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

                // recalculate message size
                msg.header.size = msg.body.size();

                // return target message so it can be "chained"
                return msg;
            }

            template <typename DataType>
            friend message<T> &operator>>(message<T> &msg, DataType &data)
            {
                // check type of data is copyable
                static_assert(std::is_standard_layout<DataType>::value, "Data is too complesx to be pushed to message buffer");

                // cache location towards end of vector where the pulled data starts
                size_t i = msg.body.size() - sizeof(DataType);

                // physically copy data from vector into user variable
                std::memcpy(&data, msg.body.data() + i, sizeof(DataType));

                // shrink vector to remove read bytes, reset end position
                msg.body.resize(i);

                // recalculate message size
                msg.header.size = msg.body.size();

                // return target message
                return msg;
            }
        };

        // forward declare connection
        template <typename T>
        class connection;

        template <typename T>
        struct owned_message
        {
            std::shared_ptr<connection<T>> remote = nullptr;
            message<T> msg;

            // friendly string maker
            friend std::ostream &operator<<(std::ostream &os, const owned_message<T> &msg)
            {
                os << msg.msg;
                return os;
            }
        };
    }
}