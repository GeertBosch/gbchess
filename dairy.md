6/11/2025: Repeated invocations of make -j take 02.6s real time

6/12/2025: Switched to c++20 from c++17, build took 4.1 sec instead of 2.8. However, I still
couldn't use the spaceship operator, as I needed it to convert to a -1/0/1 int. So reverted back to
c++17.

6/13/2025: Decided to try and use hashing for perft. Based on the birthday paradox, and the
expectation that we may visit billions of nodes, decided that while 64-bit hashes would be enough
for search it would not be OK for perft. So, I made Hash switchable to __uint128_t. As this has
almost no impact on performance, I may just keep it that way. In the table, I'm using about 20 bits
for the table index, and then 64 bits for the key to deal with collisions. This would result in an
error once in 2**((64 + 20) / 2) == 2**42 lookups, or a 0.22% chance of collission after 10B
lookups.

6/19/2025: Spent way to much time debugging the perft hashing, only to find out that I still was
using a uint32_t where I shouldn't have. There's still a possible issue when using 64-bit hashes
with 128-bit node count. I could just blame that on hash collisions, but I actually think that's
unlikely. I'll stop going down the rabbit hole any more, for now.



