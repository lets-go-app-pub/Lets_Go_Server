#pragma once

/*
 *
 * Copyright 2017 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <grpcpp/impl/codegen/call.h>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/support/sync_stream.h>

#include <utility>

namespace grpc::testing {

/*
    template<class R>
    class MockClientReader : public grpc::ClientReaderInterface<R> {
    public:
        MockClientReader() = default;

        /// ClientStreamingInterface
        MOCK_METHOD0_T(Finish, Status());

        /// ReaderInterface
        MOCK_METHOD1_T(NextMessageSize, bool(uint32_t*));
        MOCK_METHOD1_T(Read, bool(R*));

        /// ClientReaderInterface
        MOCK_METHOD0_T(WaitForInitialMetadata, void());
    };

    template<class W>
    class MockClientWriter : public grpc::ClientWriterInterface<W> {
    public:
        MockClientWriter() = default;

        /// ClientStreamingInterface
        MOCK_METHOD0_T(Finish, Status());

        /// WriterInterface
        MOCK_METHOD2_T(Write, bool(const W&, const WriteOptions));

        /// ClientWriterInterface
        MOCK_METHOD0_T(WritesDone, bool());
    };

    template<class W, class R>
    class MockClientReaderWriter : public grpc::ClientReaderWriterInterface<W, R> {
    public:
        MockClientReaderWriter() = default;

        /// ClientStreamingInterface
        MOCK_METHOD0_T(Finish, Status());

        /// ReaderInterface
        MOCK_METHOD1_T(NextMessageSize, bool(uint32_t*));
        MOCK_METHOD1_T(Read, bool(R*));

        /// WriterInterface
        MOCK_METHOD2_T(Write, bool(const W&, const WriteOptions));

        /// ClientReaderWriterInterface
        MOCK_METHOD0_T(WaitForInitialMetadata, void());
        MOCK_METHOD0_T(WritesDone, bool());
    };

    template<class R>
    class MockClientAsyncResponseReader
            : public grpc::ClientAsyncResponseReaderInterface<R> {
    public:
        MockClientAsyncResponseReader() = default;

        /// ClientAsyncResponseReaderInterface
        MOCK_METHOD0_T(StartCall, void());
        MOCK_METHOD1_T(ReadInitialMetadata, void(void * ));
        MOCK_METHOD3_T(Finish, void(R*, Status *, void *));
    };

    template<class R>
    class MockClientAsyncReader : public ClientAsyncReaderInterface<R> {
    public:
        MockClientAsyncReader() = default;

        /// ClientAsyncStreamingInterface
        MOCK_METHOD1_T(StartCall, void(void * ));
        MOCK_METHOD1_T(ReadInitialMetadata, void(void * ));
        MOCK_METHOD2_T(Finish, void(Status*, void *));

        /// AsyncReaderInterface
        MOCK_METHOD2_T(Read, void(R*, void *));
    };

    template<class W>
    class MockClientAsyncWriter : public grpc::ClientAsyncWriterInterface<W> {
    public:
        MockClientAsyncWriter() = default;

        /// ClientAsyncStreamingInterface
        MOCK_METHOD1_T(StartCall, void(void * ));
        MOCK_METHOD1_T(ReadInitialMetadata, void(void * ));
        MOCK_METHOD2_T(Finish, void(Status*, void *));

        /// AsyncWriterInterface
        MOCK_METHOD2_T(Write, void(const W&, void*));
        MOCK_METHOD3_T(Write, void(const W&, grpc::WriteOptions, void*));

        /// ClientAsyncWriterInterface
        MOCK_METHOD1_T(WritesDone, void(void * ));
    };

    template<class W, class R>
    class MockClientAsyncReaderWriter
            : public ClientAsyncReaderWriterInterface<W, R> {
    public:
        MockClientAsyncReaderWriter() = default;

        /// ClientAsyncStreamingInterface
        MOCK_METHOD1_T(StartCall, void(void * ));
        MOCK_METHOD1_T(ReadInitialMetadata, void(void * ));
        MOCK_METHOD2_T(Finish, void(Status*, void *));

        /// AsyncWriterInterface
        MOCK_METHOD2_T(Write, void(const W&, void*));
        MOCK_METHOD3_T(Write, void(const W&, grpc::WriteOptions, void*));

        /// AsyncReaderInterface
        MOCK_METHOD2_T(Read, void(R*, void *));

        /// ClientAsyncReaderWriterInterface
        MOCK_METHOD1_T(WritesDone, void(void * ));
    };

    template<class R>
    class MockServerReader : public grpc::ServerReaderInterface<R> {
    public:
        MockServerReader() = default;

        /// ServerStreamingInterface
        MOCK_METHOD0_T(SendInitialMetadata, void());

        /// ReaderInterface
        MOCK_METHOD1_T(NextMessageSize, bool(uint32_t*));
        MOCK_METHOD1_T(Read, bool(R*));
    };

    template<class W>
    class MockServerWriter : public grpc::ServerWriterInterface<W> {
    public:
        MockServerWriter() = default;

        /// ServerStreamingInterface
        MOCK_METHOD0_T(SendInitialMetadata, void());

        /// WriterInterface
        MOCK_METHOD2_T(Write, bool(const W&, const WriteOptions));
    };

    template<class W, class R>
    class MockServerReaderWriter : public grpc::ServerReaderWriterInterface<W, R> {
    public:
        MockServerReaderWriter() = default;

        /// ServerStreamingInterface
        MOCK_METHOD0_T(SendInitialMetadata, void());

        /// ReaderInterface
        MOCK_METHOD1_T(NextMessageSize, bool(uint32_t*));
        MOCK_METHOD1_T(Read, bool(R*));

        /// WriterInterface
        MOCK_METHOD2_T(Write, bool(const W&, const WriteOptions));
    };
*/

    template<class W>
    class MockServerWriterVector : public grpc::ServerWriterInterface<W> {
    public:
        MockServerWriterVector() = default;

        struct WriteParams {
            W msg;
            grpc::WriteOptions options;

            WriteParams() = delete;

            WriteParams(const WriteParams& other) {
                *this = other;
            }

            WriteParams(
                    W _msg,
                    grpc::WriteOptions options
            ) : msg(std::move(_msg)),
                options(options) {}

            WriteParams& operator=(const WriteParams& other) {
                msg = other.msg;
                options = other.options;
                return *this;
            }
        };

        std::vector<WriteParams> write_params;
        std::vector<bool> send_initial_meta_data_results;

        void SendInitialMetadata() override {
            send_initial_meta_data_results.push_back(true);
        }

        bool Write(const W& msg, grpc::WriteOptions options) override {
            write_params.emplace_back(msg, options);
            return false;
        }
    };

    template<class W, class R>
    class MockServerAsyncReaderWriter : public grpc::ServerAsyncReaderWriterInterface<W, R> {
    public:
        MockServerAsyncReaderWriter() = default;

        explicit MockServerAsyncReaderWriter(grpc::ServerContext* ctx) {};

        struct FinishParams {
            Status status;
            void* tag;

            FinishParams() = delete;

            FinishParams(
                    Status _status,
                    void* _tag
            ) : status(std::move(_status)),
                tag(_tag) {}
        };

        struct WriteAndFinishParams {
            const W msg;
            grpc::WriteOptions options;
            const Status status;
            void* tag;

            WriteAndFinishParams() = delete;

            WriteAndFinishParams(
                    W _msg,
                    grpc::WriteOptions _options,
                    Status _status,
                    void* _tag
            ) : msg(_msg),
                options(_options),
                status(std::move(_status)),
                tag(_tag) {}
        };

        struct SendInitialMetadataParams {
            void* tag;

            SendInitialMetadataParams() = delete;

            explicit SendInitialMetadataParams(
                    void* _tag
                    ) : tag(_tag) {}
        };

        struct ReadParams {
            R* msg;
            void* tag;

            ReadParams() = delete;

            explicit ReadParams(
                    R* _msg,
                    void* _tag
                    ) : msg(_msg),
                    tag(_tag) {}
        };

        struct WriteParams {
            const W msg;
            void* tag;

            WriteParams() = delete;

            WriteParams(
                    const W _msg,
                    void* _tag
                    ) : msg(_msg),
                    tag(_tag) {}
        };

        struct BindCallParams {
            internal::Call* call;

            explicit BindCallParams(
                    internal::Call* _call
                    ) : call(_call) {}
        };

        std::vector<FinishParams> finish_params;
        std::vector<WriteAndFinishParams> write_and_finish_params;
        std::vector<SendInitialMetadataParams> send_initial_metadata_params;
        std::vector<ReadParams> read_params;
        std::vector<WriteParams> write_params;
        std::vector<BindCallParams> bind_call_params;

        void Finish(const Status& status, void* tag) override {
            finish_params.emplace_back(status, tag);
        }

        void WriteAndFinish(const W& msg, grpc::WriteOptions options, const Status& status, void* tag) override {
            write_and_finish_params.emplace_back(msg, options, status, tag);
        }

        void SendInitialMetadata(void* tag) override {
            send_initial_metadata_params.emplace_back(tag);
        }

        void Read(R* msg, void* tag) override {
            read_params.emplace_back(msg, tag);
        }

        void Write(const W& msg, void* tag) override {
            write_params.emplace_back(msg, tag);
        }

        void Write(const W& msg, grpc::WriteOptions, void* tag) override {
            write_params.emplace_back(msg, tag);
        }

    private:
        void BindCall(internal::Call* call) override {
            bind_call_params.emplace_back(call);
        }

//        /// AsyncWriterInterface
//        MOCK_METHOD(void, Write, (const W& msg, void* tag));
//        MOCK_METHOD(void, Write, (const W& msg, grpc::WriteOptions options, void* tag));
//
//        /// AsyncReaderInterface
//        MOCK_METHOD(void, Read, (R* msg, void* tag));
//
//        /// ServerAsyncReaderWriterInterface
//        MOCK_METHOD(void, Finish, (const Status& status, void* tag));
//        MOCK_METHOD(void, WriteAndFinish, (const W& msg, grpc::WriteOptions options, const Status& status, void* tag));
//
//        /// ServerAsyncStreamingInterface
//        MOCK_METHOD(void, SendInitialMetadata, (void* tag));
//    private:
//        MOCK_METHOD(void, BindCall, (internal::Call* call));

    };


}  // namespace grpc

