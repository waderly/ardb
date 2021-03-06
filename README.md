# Ardb
Ardb is a BSD licensed, redis-protocol compatible persistent storage server, it support different storage engines. Currently LevelDB/LMDB/RocksDB are supported, but only LevelDB engine is well tested.


## Compile
LevelDB is the default storage engine, to compile with leveldb, just type 'make' to compile server & lib & tests.

To use LMDB/RocksDB as storage engine, you should set env 'storage_engine' first.
	
	storage_engine=lmdb make
	storage_engine=rocksdb make

It should compile to several executables in 'src' directory, such as ardb-server, ardb-test etc.
	

## Features
- Full redis-protocol compatibility
- 2d spatial index supported. [Spatial Index](https://github.com/yinqiwen/ardb/blob/develop/doc/spatial-index.md)
- Most redis commands supported, and a few new commands
  * [Ardb commands VS Redis Commands](https://github.com/yinqiwen/ardb/wiki/ARDB-Commands)
- Different storage engine supported (LevelDB/KyotoCabinet/LMDB/RocksDB)
- Replication compatible with Redis 2.6/2.8
  * Ardb instance work as slave of Redis 2.6/2.8+ instance
  * Ardb instance work as master of Redis 2.6/2.8+ instance
  * Ardb instance work as slave of Ardb instance
  * [Replication detail](https://github.com/yinqiwen/ardb/wiki/Replication)
- Auto failover support by redis-sentinel
- Backup data online
  * Use 'save/bgsave' to backup data
  * Use 'import' to import backup data
  * [Backup & Restore](https://github.com/yinqiwen/ardb/wiki/Backup-Commands)

## Client API
Since ardb is a full redis-protocol compatible server, you can use any redis client to connect it. Here lists all redis clients. <http://www.redis.io/clients>

## Benchmark
Benchmarks were all performed on a four-core Intel(R) Xeon(R) CPU E5520@2.27GHz, with 64 GB of DDR3 RAM, 500 GB of SCSI disk

The benchmark tool is 'redis-benchmark' from redis,50 parallel clients, 10000000 requests, 1000000 random keys each test case. There would be no benchmark test for RocksDB engine because of OS and compiler version limit in the test machine.

GCC Version:4.8.3  
OS Version: Red Hat Enterprise Linux AS release 4 (Nahant Update 3)   
Kernel Version: 2.6.32_1-10-6-0       
Redis Version: 2.8.9  
Ardb Version: 0.7.2(LMDB 0.9.11, LevelDB1.16.0, RocksDB3.1)  
LevelDB Options: thread_pool_size=2, block_cache_size=512m, write_buffer_size=128m, compression=snappy   
RocksDB Options: thread_pool_size=2, block_cache_size=512m, write_buffer_size=128m, compression=snappy   
LMDB Option: thread_pool_size=2, database_max_size=10G, readahead=no    

![Benchmark Img](https://raw.github.com/yinqiwen/ardb/master/doc/benchmark.png)

	Becnhmark data(./redis-benchmark -r 10000000 -n 10000000):
	                        LevelDB	  LMDB	        RocksDB	    Redis
    PING_INLINE	            95075.11	91945.56	92274.76	79669.85
    PING_BULK	            99265.43	92988.66	95721.27	90044.66
    SET	                    62361.64	72490.03	55567.91	73692.51
    GET	                    69045.99	93805.12	68078.16	82459.95
    INCR	                47572.16	59805.03	34883.32	74940.61
    LPUSH	                47369.57	27713.11	40584.42	105466.32
    LPOP	                14205.29	14711.51	10088.27	105797.72
    SADD	                37583.39	41779.82	24421.81	100405.64
    SPOP	                 109.326	17614.32	  87.634	75253.04
    LPUSH(for LRANGE)	    49504.95	16998.71	40719.93	87989.45
    LRANGE_100 	             7981.99	17639.79	7445.68  	45831.61
    LRANGE_300               3829.91	6578.86	    3113.99	    17608.11
    LRANGE_500 	             2504.61	4552.06  	2156.08     12345.07
    LRANGE_600 	             1734.72	3082.94	    1650.46	     7906.01
    MSET (10 keys)	         9675.48	7204.56	    5255.44	     35967.6


* Note: 
  - **Ardb uses 2 threads in benchmark test, while redis is single threaded application. That's the reason ardb is faster than redis in some test cases.**
  - **LevelDB & LMDB both use tree like structure on disk, more data is stored, the server is slower.**
  - **'SPOP' is very slow with LevelDB engine**
         

## Ardb vs Redis(2.8.9) 
 * Unsupported Redis Commands:
  - DUMP 
  - MIGRATE
  - OBJECT
  - RESTORE
  - CONFIG RESETSTAT
  - DEBUG
  - MONITOR
  - BITPOS
  - PUBSUB
 * Additional Commands:
  - HClear/SClear/ZClear/LClear
  - SUnionCount/SInterCount/SDiffCount
  - ZAdd with limit
  - ZPop/ZRPop
  - HMIncrby
  - \_\_SET\_\_/\_\_DEL\_\_(for replication)
  - CompactDB/CompactAll
  - BitOPCount
  - Import
  - KeysCount
  - GeoAdd
  - GeoSearch
  - Cache 

## Misc
###Memory Usage
In basho's fork of leveldb [project](https://github.com/basho/leveldb), they give a way to estimate memory usage of a database.

      write_buffer_size*2    
     + max_open_files*4194304    
     + (size in NewLRUCache initialization)  
 
If you want to limit the total memory usage, you should tweak configuration options whoose names start with 'leveldb.' in ardb.conf.
