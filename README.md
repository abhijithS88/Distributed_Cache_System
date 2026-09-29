Distributed Cache — Redis
About Redis: https://www.youtube.com/watch?v=fmT5nlEkl3U&t=50s
ABSTRACT: Redis (Remote Dictionary Server) is an in-memory key-value store widely used for
caching, session management, real-time analytics, and message brokering. It supports data persistence,
replication, and sharding, making it highly scalable for distributed workloads. Redis uses an asynchronous
event-driven architecture for high throughput and low latency. This project aims to build a distributed
caching system inspired by Redis, focusing on high availability, scalability, and low-latency access to
frequently requested data. The system will implement data partitioning, replication, eviction policies,
and a custom client API to provide efficient caching similar to Redis but without directly using Redis