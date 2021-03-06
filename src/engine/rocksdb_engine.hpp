/*
 *Copyright (c) 2013-2013, yinqiwen <yinqiwen@gmail.com>
 *All rights reserved.
 * 
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 * 
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Redis nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS 
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 *THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef ROCKSDB_ENGINE_HPP_
#define ROCKSDB_ENGINE_HPP_

#include "rocksdb/db.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/comparator.h"
#include "rocksdb/cache.h"
#include "rocksdb/filter_policy.h"

#include "db.hpp"
#include "util/config_helper.hpp"
#include "util/thread/thread_local.hpp"
#include <stack>

namespace ardb
{
    class RocksDBEngine;
    class RocksDBIterator: public Iterator
    {
        private:
            RocksDBEngine* m_engine;
            rocksdb::Iterator* m_iter;
            void Next();
            void Prev();
            Slice Key() const;
            Slice Value() const;
            bool Valid();
            void SeekToFirst();
            void SeekToLast();
            void Seek(const Slice& target);
        public:
            RocksDBIterator(RocksDBEngine* engine, rocksdb::Iterator* iter) :
                    m_engine(engine), m_iter(iter)
            {

            }
            ~RocksDBIterator();
    };

    class RocksDBComparator: public rocksdb::Comparator
    {
        public:
            // Three-way comparison.  Returns value:
            //   < 0 iff "a" < "b",
            //   == 0 iff "a" == "b",
            //   > 0 iff "a" > "b"
            int Compare(const rocksdb::Slice& a, const rocksdb::Slice& b) const;

            // The name of the comparator.  Used to check for comparator
            // mismatches (i.e., a DB created with one comparator is
            // accessed using a different comparator.
            //
            // The client of this package should switch to a new name whenever
            // the comparator implementation changes in a way that will cause
            // the relative ordering of any two keys to change.
            //
            // Names starting with "RocksDB." are reserved and should not be used
            // by any clients of this package.
            const char* Name() const
            {
                return "ARDBRocksDB";
            }

            // Advanced functions: these are used to reduce the space requirements
            // for internal data structures like index blocks.

            // If *start < limit, changes *start to a short string in [start,limit).
            // Simple comparator implementations may return with *start unchanged,
            // i.e., an implementation of this method that does nothing is correct.
            void FindShortestSeparator(std::string* start, const rocksdb::Slice& limit) const;

            // Changes *key to a short string >= *key.
            // Simple comparator implementations may return with *key unchanged,
            // i.e., an implementation of this method that does nothing is correct.
            void FindShortSuccessor(std::string* key) const;
    };

    struct RocksDBConfig
    {
            std::string path;
            int64 block_cache_size;
            int64 block_cache_compressed_size;
            int64 write_buffer_size;
            int64 max_open_files;
            int64 block_size;
            int64 block_restart_interval;
            int64 bloom_bits;
            int64 batch_commit_watermark;
            std::string compression;
            RocksDBConfig() :
                    block_cache_size(0), block_cache_compressed_size(0), write_buffer_size(0), max_open_files(10240), block_size(
                            0), block_restart_interval(0), bloom_bits(10), batch_commit_watermark(1024), compression(
                            "snappy")
            {
            }
    };
    class RocksDBEngineFactory;
    class RocksDBEngine: public KeyValueEngine
    {
        private:
            rocksdb::DB* m_db;
            RocksDBComparator m_comparator;
            struct ContextHolder
            {
                    rocksdb::WriteBatch batch;
                    uint32 ref;
                    uint32 count;
                    const rocksdb::Snapshot* snapshot;
                    uint32 snapshot_ref;

                    void ReleaseRef()
                    {
                        if (ref > 0)
                            ref--;
                    }
                    void AddRef()
                    {
                        ref++;
                    }
                    bool EmptyRef()
                    {
                        return ref == 0;
                    }
                    void Clear()
                    {
                        batch.Clear();
                        count = 0;
                    }
                    void Put(const Slice& key, const Slice& value);
                    void Del(const Slice& key);
                    ContextHolder() :
                            ref(0), count(0), snapshot(NULL), snapshot_ref(0)
                    {
                    }
            };
            ThreadLocal<ContextHolder> m_context;
            std::string m_db_path;

            RocksDBConfig m_cfg;
            rocksdb::Options m_options;
            friend class RocksDBEngineFactory;
            int FlushWriteBatch(ContextHolder& holder);
        public:
            RocksDBEngine();
            ~RocksDBEngine();
            int Init(const RocksDBConfig& cfg);
            int Put(const Slice& key, const Slice& value);
            int Get(const Slice& key, std::string* value, bool fill_cache);
            int Del(const Slice& key);
            int BeginBatchWrite();
            int CommitBatchWrite();
            int DiscardBatchWrite();
            Iterator* Find(const Slice& findkey, bool cache);
            const std::string Stats();
            void CompactRange(const Slice& begin, const Slice& end);
            void ReleaseContextSnapshot();
    };

    class RocksDBEngineFactory: public KeyValueEngineFactory
    {
        private:
            RocksDBConfig m_cfg;
            static void ParseConfig(const Properties& props, RocksDBConfig& cfg);
        public:
            RocksDBEngineFactory(const Properties& cfg);
            KeyValueEngine* CreateDB(const std::string& name);
            void DestroyDB(KeyValueEngine* engine);
            void CloseDB(KeyValueEngine* engine);
            const std::string GetName()
            {
                return "RocksDB";
            }
    };
}
#endif
