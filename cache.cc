
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "cache.h"
using namespace std;

Cache::Cache(int s,int a,int b )
{
   
   ulong i, j;

    lineSize = (ulong)(b);
    sets = 1;               // Only one set for fully associative
    assoc = 10000;        // Large associativity to simulate "infinite" cache
    numLines = assoc;       // The number of lines is equal to associativity
    //log2Blk = log2(b);
   
 
cache = new cacheLine*[sets];
    for (i = 0; i < sets; i++) {
        cache[i] = new cacheLine[assoc];
        for (j = 0; j < assoc; j++) {
            cache[i][j].invalidate();
        }
    }    
   
}



void Cache::MESI_Processor_Access(ulong addr, uchar rw, int copy, Cache** cache, int processor, int num_processors) 
{
    if (rw == 'r') {
        reads++;
        cacheLine* target_line = cache[processor]->findLine(addr);

        if (target_line != nullptr) { // Cache hit
            Readhits++;
            Total_execution_time += read_hit_latency;
            ulong current_state = target_line->getFlags();

            if (current_state == Exclusive) {
                target_line->setFlags(Exclusive);
            } else if (current_state == Modified) {
                target_line->setFlags(Modified);
            } else if (current_state == Shared) {
                target_line->setFlags(Shared);
            }
        } else { // Cache miss
            readMisses++;
            target_line = cache[processor]->fillLine(addr);

            if (copy == 0) {
                target_line->setFlags(Exclusive);
                mem_trans++;
                Total_execution_time += memory_latency;
            } else {
                target_line->setFlags(Shared);
                Total_execution_time += flush_transfer;
            }

            for (int i = 0; i < num_processors; i++) {
                if (i != processor) {
                    cache[i]->MESI_Bus_Snoop(addr, 1, 0, 0);
                }
            }
        }
    } else if (rw == 'w') {
        writes++;
        cacheLine* target_line = cache[processor]->findLine(addr);

        if (target_line == nullptr) { // Cache miss
            writeMisses++;
            target_line = cache[processor]->fillLine(addr);

            if (copy) {
                Total_execution_time += flush_transfer;
            } else {
                mem_trans++;
                Total_execution_time += memory_latency;
            }

            target_line->setFlags(Modified);

            for (int i = 0; i < num_processors; i++) {
                if (i != processor) {
                    cache[i]->MESI_Bus_Snoop(addr, 0, 1, 0);
                }
            }
        } else { // Cache hit
            Writehits++;
            Total_execution_time += write_hit_latency;
            ulong current_state = target_line->getFlags();

            if (current_state == Modified) {
                // State remains Modified
            } else if (current_state == Shared) {
                target_line->setFlags(Modified);
                for (int i = 0; i < num_processors; i++) {
                    if (i != processor) {
                        cache[i]->MESI_Bus_Snoop(addr, 0, 0, 1);
                    }
                }
            } else if (current_state == Exclusive) {
                target_line->setFlags(Modified);
            }
        }
    }
}


void Cache::MESI_Bus_Snoop(ulong addr, int busread, int busreadx, int busupgrade) 
{
    // Locate the cache line corresponding to the address
    cacheLine* line = findLine(addr);
    if (line == nullptr) return; // If not found, no action needed

    ulong current_state = line->getFlags();

    // Handle Bus Read Exclusive (BusRdX) operation
    if (busreadx) {
        line->setFlags(INVALID);
        invalidations++;
        flushes++;
    } 
    // Handle Bus Read (BusRd) operation
    else if (busread) {
        if (current_state == Modified || current_state == Exclusive) {
            line->setFlags(Shared);
            flushes++;
        } 
        else if (current_state == Shared) {
            flushes++;
        }
    } 
    // Handle Bus Upgrade (BusUpgr) operation
    else if (busupgrade) {
        line->setFlags(INVALID);
        invalidations++;
    }
}


void Cache::MOESI_Processor_Access(ulong addr, uchar rw, int copy, Cache** cache, int processor, int num_processors) 
{
    // Update counters based on operation type
    (rw == 'r') ? reads++ : writes++;

    // Look for the line in the processor's cache
    cacheLine* target_line = cache[processor]->findLine(addr);

    if (target_line == nullptr) { // Cache miss
        if (rw == 'r') { // Handle read miss
            readMisses++;
            target_line = cache[processor]->fillLine(addr);

            if (copy) {
                target_line->setFlags(Shared);
                Total_execution_time += flush_transfer;
            } else {
                target_line->setFlags(Exclusive);
                mem_trans++;
                Total_execution_time += memory_latency;
            }

            // Notify other processors about the BusRd
            for (int i = 0; i < num_processors; i++) {
                if (i != processor) {
                    cache[i]->MOESI_Bus_Snoop(addr, 1, 0, 0);
                }
            }
        } else { // Handle write miss
            writeMisses++;
            target_line = cache[processor]->fillLine(addr);
            target_line->setFlags(Modified);

            if (!copy) {
                mem_trans++;
                Total_execution_time += memory_latency;
            } else {
                Total_execution_time += flush_transfer;
            }

            // Notify other processors about the BusRdX
            for (int i = 0; i < num_processors; i++) {
                if (i != processor) {
                    cache[i]->MOESI_Bus_Snoop(addr, 0, 1, 0);
                }
            }
        }
    } else { // Cache hit
        if (rw == 'r') { // Handle read hit
            Readhits++;
            Total_execution_time += read_hit_latency;
        } else { // Handle write hit
            Writehits++;
            Total_execution_time += write_hit_latency;

            ulong current_state = target_line->getFlags();
            target_line->setFlags(Modified);

            // If the state is Shared or Owner, notify other processors about BusUpgr
            if (current_state == Shared || current_state == Owner) {
                for (int i = 0; i < num_processors; i++) {
                    if (i != processor) {
                        cache[i]->MOESI_Bus_Snoop(addr, 0, 0, 1);
                    }
                }
            }
        }
    }
}


void Cache::MOESI_Bus_Snoop(ulong addr, int busread, int busreadx, int busupgrade) 
{
    // Locate the cache line
    cacheLine* line = findLine(addr);
    if (line == nullptr) {
        return; // No action needed if the line is not found
    }

    ulong current_state = line->getFlags();

    if (busupgrade) { // Handle Bus Upgrade (BusUpgr)
        if (current_state == Shared || current_state == Owner) {
            line->setFlags(INVALID);
            invalidations++;
        }
    } else if (busreadx) { // Handle Bus Read Exclusive (BusRdX)
        if (current_state == Exclusive) {
            c_to_c_trans++;
        }
        if (current_state == Modified || current_state == Owner) {
            flushes++;
        }
        line->setFlags(INVALID);
        invalidations++;
    } else if (busread) { // Handle Bus Read (BusRd)
        if (current_state == Modified || current_state == Owner) {
            flushes++;
        }
        if (current_state == Modified) {
            line->setFlags(Owner);
        } else if (current_state == Exclusive) {
            line->setFlags(Shared);
        }
        if (current_state == Shared || current_state == Exclusive) {
            c_to_c_trans++;
        }
    }
}


/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++)
	if(cache[i][j].isValid())
	        if(cache[i][j].getTag() == tag)
		{
		     pos = j; break; 
		}
   if(pos == assoc)
	return NULL;
   else
	return &(cache[i][pos]); 
}


/*Most of these functions are redundant so you can use/change it if you want to*/

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
  line->setSeq(currentCycle);  
  currentCycle++;
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) 
	  return &(cache[i][j]);     
   }   
   for(j=0;j<assoc;j++)
   {
	 if(cache[i][j].getSeq() <= min) { victim = j; min = cache[i][j].getSeq();}
   } 
   assert(victim != assoc);
   std::cout << "victim" << victim << std::endl;
   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{ 
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);

   assert(victim != 0);
   if ((victim->getFlags() == Modified))
   {
	   writeBack(addr);
   }
   victim->setFlags(Shared);	
   tag = calcTag(addr);   
   victim->setTag(tag);
       
   return victim;
}

void Cache::printStats(int cache_id, const char* trace_file, int num_processors, const char* protocol, ulong total_execution_time)
{ 
	std::cout << "============ Simulation results (Cache " << cache_id << ") ===========" << std::endl;
	float miss_rate = (float)(getRM() + getWM()) * 100 / (getReads() + getWrites());
	
printf("01. number of reads:                                 %10lu\n", getReads());
printf("02. number of read misses:                           %10lu\n", getRM());
printf("03. number of writes:                                %10lu\n", getWrites());
printf("04. number of write misses:                          %10lu\n", getWM());
printf("05. number of write hits:                            %10lu\n", getWH());
printf("06. number of read hits:                             %10lu\n", getRH());
printf("07. total miss rate:                                 %10.2f%%\n", miss_rate);
printf("08. number of memory accesses (exclude writebacks):  %10lu\n", mem_trans);
printf("09. number of invalidations:                         %10lu\n", Invalidations());
printf("10. number of flushes:                               %10lu\n", Flushes());

	
}

void Cache::printCacheState(ulong state) {
    switch (state) {
        case INVALID:
            std::cout << "I";
            break;
        case Shared:
            std::cout << "S";
            break;
        case Modified:
            std::cout << "M";
            break;
        case Exclusive:
            std::cout << "E";
            break;
        default:
            std::cout << "-";
            break;
    }
}
