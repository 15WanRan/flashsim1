#include "../ssd.h"
#include "../block_management.h"
using namespace ssd;

unsigned long previous_erased_addr = 0;

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
{}

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
{}

uint Garbage_Collector_Super::get_superblock_idx(Address const& phys_address){
	// left bits
	return phys_address.block>>8;
	// return 0;
}

void Garbage_Collector_Super::register_event_completion(Event & event) {
	//////////////////////////////////////////
	//invalidate -> mvl update
	//////////////////////////////////////////

	if(event.get_event_type() != WRITE){
		return;
	}
	Address ra = event.get_replace_address();
	if (ra.valid == NONE) {
		// no invalidate!!!  ->  ignore this event
		return;
	}
	// mark as invalid!!!
	ra.valid = BLOCK;
	ra.page = 0;

	// let's see the block state containing ra
	Block& block = *ssd -> get_package(ra.package)->get_die(ra.die)->get_plane(ra.plane)->get_block(ra.block);


	assert(block.get_pages_invalid()>0);
	uint new_level = block.get_pages_invalid() - 1; 

	// insert to (new level) list 
	unsigned long mvl_item=ra.get_linear_address();

	uint superblock_idx = get_superblock_idx(ra);
	superblock_mvl[ra.package][ra.die][superblock_idx][new_level].insert(mvl_item);

	if(new_level>0){
		superblock_mvl[ra.package][ra.die][superblock_idx][new_level-1].erase(mvl_item);
	}

	// superblock metadata update
	superblock_invalid[ra.package][ra.die][superblock_idx]++;
}

void Garbage_Collector_Super::erase_event_completion(Event & event) {
	// 1. mvl update
	// 2. superblock metadata update

	// 1. mvl update
	Address ra = event.get_address();


	int package = ra.package;
	int die = ra.die;

	uint superblock_idx = get_superblock_idx(ra);

	Block& victim_block = *ssd -> get_package(package)->get_die(die)->get_plane(ra.plane)->get_block(ra.block);
	int level_of_block = victim_block.get_last_pages_invalid()-1;

	// erase from MVL!!! 
	unsigned long mvl_item=ra.get_linear_address();
	superblock_mvl[package][die][superblock_idx][level_of_block].erase(mvl_item);
	victim_block.set_last_pages_invalid(0);

	// 2. superblock metadata update

	if(event.erased_invalid > 0  && previous_erased_addr != ra.get_linear_address()){

		previous_erased_addr=ra.get_linear_address();

		Block& block = *ssd -> get_package(ra.package)->get_die(ra.die)->get_plane(ra.plane)->get_block(ra.block);
		uint superblock_idx = get_superblock_idx(ra);

		int minus_invalid=event.erased_invalid;
		event.erased_invalid=0;

		superblock_invalid[ra.package][ra.die][superblock_idx]-=minus_invalid;

		if(superblock_pe[ra.package][ra.die][superblock_idx]<block.get_age()){
			superblock_pe[ra.package][ra.die][superblock_idx]=block.get_age();
			superblock_max_pe = (superblock_max_pe>block.get_age()) ?  superblock_max_pe : block.get_age();
		}
	}
}




//victim selection (candi blocks)
vector<long> Garbage_Collector_Super::get_relevant_gc_candidates(int package_id, int die_id, int klass) const {
	vector<long > candidates;
	int package=package_id;
	int die=die_id;


	if (package == UNDEFINED) {
		package = rand() % SSD_SIZE;
	}
	if (die == UNDEFINED) {
		die = rand() % PACKAGE_SIZE;
	}

	//  select the best superblock
	int best_superblock = 0;
	best_superblock = rand()%SUPERBLOCK_SIZE;
	int best_score = superblock_pe[package][die][best_superblock]+superblock_invalid[package][die][best_superblock];
	for(int superblock_i=0;superblock_i<SUPERBLOCK_SIZE;superblock_i++){
		//////////////////////////////////////
		// improve policy!!!!
		///////////////////////////////////////////
		if(best_score<superblock_pe[package][die][superblock_i]+superblock_invalid[package][die][superblock_i]){
			best_superblock=superblock_i;
			best_score=superblock_pe[package][die][superblock_i]+superblock_invalid[package][die][superblock_i];
		}
	}
	//fill candidates vector
	for(int mvl_level=BLOCK_SIZE-1;mvl_level>=0;mvl_level--){
		if(superblock_mvl[package][die][best_superblock][mvl_level].size()>=1){
			long best_block_linear_address = *superblock_mvl[package][die][best_superblock][mvl_level].begin();
			candidates.push_back(best_block_linear_address);
			
			// assert
			Address a = Address(best_block_linear_address, BLOCK);
			Block* best_block = ssd->get_package(a.package)->get_die(a.die)->get_plane(a.plane)->get_block(a.block);
			if (best_block->get_pages_invalid()!=mvl_level+1){
			}
			// assert();
			break;
		}
	}

	return candidates;
}

//victim selection (best block)
Block* Garbage_Collector_Super::choose_gc_victim(int package_id, int die_id, int klass) const {
	// return NULL;
	vector<long> candidates;
	candidates = get_relevant_gc_candidates(package_id, die_id, klass);

	long best_block_linear_address=candidates[0];
	Address a = Address(best_block_linear_address, BLOCK);
	Block* best_block = ssd->get_package(a.package)->get_die(a.die)->get_plane(a.plane)->get_block(a.block);

	return best_block;
}

// fix-up mvl since victim block should be moved
void Garbage_Collector_Super::commit_choice_of_victim(Address const& phys_address, double time) {

	// int package = phys_address.package;
	// int die = phys_address.die;

	// uint superblock_idx = get_superblock_idx(phys_address);

	// Block& victim_block = *ssd -> get_package(package)->get_die(die)->get_plane(phys_address.plane)->get_block(phys_address.block);
	// int level_of_block = victim_block.get_last_pages_invalid()-1;

	// // erase from MVL!!! 
	// unsigned long mvl_item=phys_address.get_linear_address();
	// set<long>::iterator it = superblock_mvl[package][die][superblock_idx][level_of_block].find(mvl_item);
	// if (it != superblock_mvl[package][die][superblock_idx][level_of_block].end()) {
	// }else{
	// }
	// superblock_mvl[package][die][superblock_idx][level_of_block].erase(mvl_item);
	
	// it = superblock_mvl[package][die][superblock_idx][level_of_block].find(mvl_item);
	// if (it == superblock_mvl[package][die][superblock_idx][level_of_block].end()) {
	// }
	// victim_block.set_last_pages_invalid(0);
}


