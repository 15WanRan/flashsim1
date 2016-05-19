#include "../ssd.h"
#include "../block_management.h"
using namespace ssd;

unsigned long previous_erased_addr = 0;
unsigned long previous_invalidated_addr = 0;
int call_time=0;
Garbage_Collector_Super::Garbage_Collector_Super()
:  Garbage_Collector(),
   gc_candidates(SSD_SIZE, vector<vector<set<long> > > (PACKAGE_SIZE, vector<set<long> >(SUPERBLOCK_SIZE, set<long>()))),
   superblock_pe(SSD_SIZE, vector<vector<long > > (PACKAGE_SIZE, vector<long >(SUPERBLOCK_SIZE, 0))),
   superblock_invalid(SSD_SIZE, vector<vector<long > > (PACKAGE_SIZE, vector<long >(SUPERBLOCK_SIZE, 0))),
   superblock_mvl(
   	SSD_SIZE,
   	vector<vector<vector<set<long>  > > > (
   		PACKAGE_SIZE,
   		vector<vector<set<long> > >(
   			SUPERBLOCK_SIZE,
   			vector<set<long> > (
   				BLOCK_SIZE,
   				set<long>()
   			)
   		)
   	)
   )


{
}

Garbage_Collector_Super::Garbage_Collector_Super(Ssd* ssd, Block_manager_parent* bm)
:  Garbage_Collector(ssd, bm),
   gc_candidates(SSD_SIZE, vector<vector<set<long> > > (PACKAGE_SIZE, vector<set<long> >(SUPERBLOCK_SIZE, set<long>()))),
   superblock_pe(SSD_SIZE, vector<vector<long > > (PACKAGE_SIZE, vector<long >(SUPERBLOCK_SIZE, 0))),
   superblock_invalid(SSD_SIZE, vector<vector<long > > (PACKAGE_SIZE, vector<long >(SUPERBLOCK_SIZE, 0))),
    superblock_mvl(
   	SSD_SIZE,
   	vector<vector<vector<set<long>  > > > (
   		PACKAGE_SIZE,
   		vector<vector<set<long> > >(
   			SUPERBLOCK_SIZE,
   			vector<set<long> > (
   				BLOCK_SIZE,
   				set<long>()
   			)
   		)
   	)
   )
   
   
//gc_candidates(SSD_SIZE, vector<set<long> >(PACKAGE_SIZE, set<long>()))

{
}


uint Garbage_Collector_Super::get_superblock_idx(Address const& phys_address){
//	return phys_address.block>>10;
	return 0;
}

void Garbage_Collector_Super::commit_choice_of_victim(Address const& phys_address, double time) {
	fprintf(stdout, "commit_choice_of_victim()\n");
	/*
	uint superblock_idx = get_superblock_idx(phys_address);
	gc_candidates[phys_address.package][phys_address.die][superblock_idx].erase(phys_address.get_linear_address());

	if( superblock_pe[phys_address.package][phys_address.die][superblock_idx]  ){
		//compare and update superblock_pe if new pe is bigger than
	}
	 */
}

vector<long> Garbage_Collector_Super::get_relevant_gc_candidates(int package_id, int die_id, int klass) const {

	vector<long > candidates;
	int package=package_id;
	int die=die_id;

	fprintf(stdout, "get_relevant_gc_candidates()\n");

	if (package == UNDEFINED) {
		package = rand() % SSD_SIZE;
	}
	if (die == UNDEFINED) {
		die = rand() % PACKAGE_SIZE;
	}

	//  select the best superblock
	int best_superblock = 0;
	int best_score = superblock_pe[package][die][best_superblock]+superblock_invalid[package][die][best_superblock];
	for(int superblock_i=0;superblock_i<SUPERBLOCK_SIZE;superblock_i++){
		// improve policy!!!!
		if(best_score<superblock_pe[package][die][superblock_i]+superblock_invalid[package][die][superblock_i]){
			best_superblock=superblock_i;
			best_score=superblock_pe[package][die][superblock_i]+superblock_invalid[package][die][superblock_i];
		}
	}

	fprintf(stdout, "get_relevant_gc_candidates1()\n");

	for(int i=BLOCK_SIZE-1;i>=0;i--){
		if(superblock_mvl[package][die][best_superblock][i].size()>=1){
			set<long>::iterator it;
			for (it=superblock_mvl[package][die][best_superblock][i].begin();it!=superblock_mvl[package][die][best_superblock][i].end();++it){
				long best_block_linear_address = *superblock_mvl[package][die][best_superblock][i].begin();
				Address a = Address(best_block_linear_address, BLOCK);
				Block* best_block = ssd->get_package(a.package)->get_die(a.die)->get_plane(a.plane)->get_block(a.block);
				if(best_block->get_pages_invalid()!=i){
					fprintf(stdout, "sibal~~~%d %d %d %d\n", best_block->get_pages_invalid(), i+1, best_block->get_pages_invalid()-i-1, superblock_mvl[package][die][best_superblock][i].size());
				}
			}
		 	long best_block_linear_address = *superblock_mvl[package][die][best_superblock][i].begin();
			Address a = Address(best_block_linear_address, BLOCK);
			Block* best_block = ssd->get_package(a.package)->get_die(a.die)->get_plane(a.plane)->get_block(a.block);
//			if(best_block->get_pages_invalid()!=i+1){
//				fprintf(stdout, "sibal~~~%d %d %d %d\n", best_block->get_pages_invalid(), i+1, best_block->get_pages_invalid()-i-1, superblock_mvl[package][die][best_superblock][i].size());

			//candidates fill up!
			candidates.push_back(best_block_linear_address);

			// mvl fixup
//			superblock_mvl[package][die][best_superblock][i].erase(best_block_linear_address);
//			break;
		}
	}

	fprintf(stdout, "get_relevant_gc_candidates2()\n");

	if (candidates.size() != 1) {
		fprintf(stdout, "candidate size: %d\n", candidates.size());
	}
		
	return candidates;
}

Block* Garbage_Collector_Super::choose_gc_victim(int package_id, int die_id, int klass) const {
	vector<long> candidates;
	fprintf(stdout, "choose_gc_victim()\n");
	candidates = get_relevant_gc_candidates(package_id, die_id, klass);

	long best_block_linear_address=candidates[0];
	Address a = Address(best_block_linear_address, BLOCK);
	Block* best_block = ssd->get_package(a.package)->get_die(a.die)->get_plane(a.plane)->get_block(a.block);

	return best_block;
}



void Garbage_Collector_Super::erase_event_completion(Event & event) {
	printf("erase_event start\n");
	Address ra = event.get_address();


	if(event.erased_invalid > 0  && previous_erased_addr != ra.get_linear_address()){
		previous_erased_addr=ra.get_linear_address();

		Block& block = *ssd -> get_package(ra.package)->get_die(ra.die)->get_plane(ra.plane)->get_block(ra.block);
		uint superblock_idx = get_superblock_idx(ra);

		int minus_invalid=event.erased_invalid;
		superblock_mvl[ra.package][ra.die][superblock_idx][minus_invalid].erase(ra.get_linear_address());
		event.erased_invalid=0;

		superblock_invalid[ra.package][ra.die][superblock_idx]-=minus_invalid;

		if(superblock_pe[ra.package][ra.die][superblock_idx]<block.get_age()){
			superblock_pe[ra.package][ra.die][superblock_idx]=block.get_age();
			superblock_max_pe = (superblock_max_pe>block.get_age()) ?  superblock_max_pe : block.get_age();
		}
	}
	printf("erase_event end\n");
}

void Garbage_Collector_Super::register_event_completion(Event & event) {
	printf("gc register_event_completion start \n");
	Address ra = event.get_replace_address();


	if (event.get_event_type() != WRITE) {
		printf("gc register_event_completion end (no write) \n");
		return;
	}
// need to revive
	// if(event.invalidate_page_flag){
	// 	previous_invalidated_addr=ra.get_linear_address();
	// 	uint superblock_idx = get_superblock_idx(ra);
	// 	superblock_invalid[ra.package][ra.die][superblock_idx]++;
	// 	event.invalidate_page_flag=0;

	// }

	uint superblock_idx = get_superblock_idx(ra);
	//sim edit
	Block& block = *ssd -> get_package(ra.package)->get_die(ra.die)->get_plane(ra.plane)->get_block(ra.block);
	int new_level = block.get_pages_invalid();
	superblock_mvl[ra.package][ra.die][superblock_idx][new_level].insert(ra.get_linear_address());
	if(new_level>1){
		// pop  from  original  set (get_pages_invalid()-1)
		superblock_mvl[ra.package][ra.die][superblock_idx][new_level-1].erase(ra.get_linear_address());
	}

	if (ra.valid == NONE) {
		printf("gc register_event_completion end (no valid) \n");
		return;
	}
	ra.valid = BLOCK;
	ra.page = 0;

	printf("gc register_event_completion end \n");

//
//	if(superblock_mvl[ra.package][ra.die][superblock_idx][new_level].size()==1){
//		if(call_time++%1000==0)
//			printf("call time %d\n", call_time);
//		bm->check_if_should_trigger_more_GC(event);
//
//	}
}
