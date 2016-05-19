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
	// return phys_address.block>>10;
	return 0;
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

	// printf("Garbage_Collector_Super::register_event_completion start \n");
	// let's see the block state containing ra
	Block& block = *ssd -> get_package(ra.package)->get_die(ra.die)->get_plane(ra.plane)->get_block(ra.block);


	assert(block.get_pages_invalid()>0);
	// level 0  => invalid 1
	// level BLOCK_SIZE-1 => invalid BLOCK_SIZE
	uint new_level = block.get_pages_invalid() - 1; 

	// insert to (new level) list 
	unsigned long mvl_item=ra.get_linear_address();

	uint superblock_idx = get_superblock_idx(ra);
	superblock_mvl[ra.package][ra.die][superblock_idx][new_level].insert(mvl_item);

	if (mvl_item == 248320) {
		printf("?. 248320 invlaid %d new level%d\n", block.get_pages_invalid(), new_level);
	} 

	// if(mvl_item==248320){
	// 	//insert result
	// 	set<long>::iterator it = superblock_mvl[ra.package][ra.die][superblock_idx][new_level].find(mvl_item);
	// 	printf("insert result=%d  , level(%d) start\n",   it != superblock_mvl[ra.package][ra.die][superblock_idx][new_level].end(), new_level);
	// }

	if(new_level>0){
		//  pop from (new level-1) list
		// set<long>::iterator it = superblock_mvl[ra.package][ra.die][superblock_idx][new_level-1].find(mvl_item);
		// if(mvl_item==248320){
		// 	printf("assert test %lu %d start\n", mvl_item, new_level-1);
		// }
		// // assert(it != superblock_mvl[ra.package][ra.die][superblock_idx][new_level-1].end());
		// if(mvl_item==248320){
		// 	printf("assert test %lu %d end\n", mvl_item, new_level-1);
		// }
		superblock_mvl[ra.package][ra.die][superblock_idx][new_level-1].erase(mvl_item);
	}

	// printf("Garbage_Collector_Super::register_event_completion end \n");

	// would it be required???? I am not sure
	// if (gc_candidates[ra.package][ra.die].size() == 1) {
	// 	if(call_time_g++%1000==0)
	// 		printf("call time %d\n", call_time_g);
	// 	bm->check_if_should_trigger_more_GC(event);
	// }
}





//userd in migrator
//    42  	if (event->get_event_type() == ERASE) {
//    43  		if(event->erased_invalid > 0) {
//    44: 			gc->erase_event_completion(*event);
//    45  		}
void Garbage_Collector_Super::erase_event_completion(Event & event) {
	// hmm???? hi again

	Address phys_address = event.get_address();
	printf("\t\t3. commit_choice_of_victim start\n");


	int package = phys_address.package;
	int die = phys_address.die;

	printf("\t\t3. addr %lu\n", phys_address.get_linear_address());
	uint superblock_idx = get_superblock_idx(phys_address);

	Block& victim_block = *ssd -> get_package(package)->get_die(die)->get_plane(phys_address.plane)->get_block(phys_address.block);
	printf("\t\t3. victim_block block %ld\n", &victim_block);
	int level_of_block = victim_block.get_last_pages_invalid()-1;
	printf("\t\t3. level_of_block  %d\n", level_of_block);

	// erase from MVL!!! 
	unsigned long mvl_item=phys_address.get_linear_address();
	set<long>::iterator it = superblock_mvl[package][die][superblock_idx][level_of_block].find(mvl_item);
	if (it != superblock_mvl[package][die][superblock_idx][level_of_block].end()) {
		printf("\t\t3. !<!>!<>!<!<!<!><!><!><!><!><!containing!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}else{
		printf("\t\t3. !<!>!<>!<!<!<!><!><!><!><!><!SIBAL?????? i is not in set!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}
	superblock_mvl[package][die][superblock_idx][level_of_block].erase(mvl_item);
	
	it = superblock_mvl[package][die][superblock_idx][level_of_block].find(mvl_item);
	if (it == superblock_mvl[package][die][superblock_idx][level_of_block].end()) {
		printf("\t\t3. !<!>!<>!<!<!<!><!><!><!><!><!DELETE SUCESS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}
	victim_block.set_last_pages_invalid(0);
	printf("\t\t3. commit_choice_of_victim end\n");
}




//victim selection (candi blocks)
vector<long> Garbage_Collector_Super::get_relevant_gc_candidates(int package_id, int die_id, int klass) const {
	vector<long > candidates;
	int package=package_id;
	int die=die_id;

	fprintf(stdout, "1. get_relevant_gc_candidates start\n");

	if (package == UNDEFINED) {
		package = rand() % SSD_SIZE;
	}
	if (die == UNDEFINED) {
		die = rand() % PACKAGE_SIZE;
	}

	//  select the best superblock
	int best_superblock = 0;
	// int best_score = superblock_pe[package][die][best_superblock]+superblock_invalid[package][die][best_superblock];
	// for(int superblock_i=0;superblock_i<SUPERBLOCK_SIZE;superblock_i++){
	// 	//////////////////////////////////////
	// 	// improve policy!!!!
	// 	///////////////////////////////////////////
	// 	if(best_score<superblock_pe[package][die][superblock_i]+superblock_invalid[package][die][superblock_i]){
	// 		best_superblock=superblock_i;
	// 		best_score=superblock_pe[package][die][superblock_i]+superblock_invalid[package][die][superblock_i];
	// 	}
	// }
	
	//fill candidates vector
	for(int mvl_level=BLOCK_SIZE-1;mvl_level>=0;mvl_level--){
		if(superblock_mvl[package][die][best_superblock][mvl_level].size()>=1){
			long best_block_linear_address = *superblock_mvl[package][die][best_superblock][mvl_level].begin();
			candidates.push_back(best_block_linear_address);
			
			// assert
			Address a = Address(best_block_linear_address, BLOCK);
			Block* best_block = ssd->get_package(a.package)->get_die(a.die)->get_plane(a.plane)->get_block(a.block);
			if (best_block->get_pages_invalid()!=mvl_level+1){
				printf("1. [wrong] addr:%lu invalid:%d level:%d\n",a.get_linear_address(), best_block->get_pages_invalid(), mvl_level+1);
			}
			// assert();
			break;
		}
	}
	fprintf(stdout, "1. get_relevant_gc_candidates end\n");

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
	printf("\t2.  victim invalid %lu %d\n", best_block_linear_address, best_block->get_pages_invalid());

	return best_block;
}



// void Migrator::update_structures(Address const& a, double time) {
// 	Block* victim = ssd->get_package(a.package)->get_die(a.die)->get_plane(a.plane)->get_block(a.block);
// 	gc->commit_choice_of_victim(a, time);
// 	blocks_being_garbage_collected[victim->get_physical_address()] = victim->get_pages_valid();
// 	num_blocks_being_garbaged_collected_per_LUN[a.package][a.die]++;
// 	StatisticsGatherer::get_global_instance()->register_executed_gc(*victim);
// }


// fix-up mvl since victim block should be moved
void Garbage_Collector_Super::commit_choice_of_victim(Address const& phys_address, double time) {
	// printf("\t\t3. commit_choice_of_victim start\n");

	// int package = phys_address.package;
	// int die = phys_address.die;

	// printf("\t\t3. addr %lu\n", phys_address.get_linear_address());
	// uint superblock_idx = get_superblock_idx(phys_address);

	// Block& victim_block = *ssd -> get_package(package)->get_die(die)->get_plane(phys_address.plane)->get_block(phys_address.block);
	// printf("\t\t3. victim_block block %ld\n", &victim_block);
	// int level_of_block = victim_block.get_last_pages_invalid()-1;
	// printf("\t\t3. level_of_block  %d\n", level_of_block);

	// // erase from MVL!!! 
	// unsigned long mvl_item=phys_address.get_linear_address();
	// set<long>::iterator it = superblock_mvl[package][die][superblock_idx][level_of_block].find(mvl_item);
	// if (it != superblock_mvl[package][die][superblock_idx][level_of_block].end()) {
	// 	printf("\t\t3. !<!>!<>!<!<!<!><!><!><!><!><!containing!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	// }else{
	// 	printf("\t\t3. !<!>!<>!<!<!<!><!><!><!><!><!SIBAL?????? i is not in set!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	// }
	// superblock_mvl[package][die][superblock_idx][level_of_block].erase(mvl_item);
	
	// it = superblock_mvl[package][die][superblock_idx][level_of_block].find(mvl_item);
	// if (it == superblock_mvl[package][die][superblock_idx][level_of_block].end()) {
	// 	printf("\t\t3. !<!>!<>!<!<!<!><!><!><!><!><!DELETE SUCESS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	// }
	// victim_block.set_last_pages_invalid(0);
	// printf("\t\t3. commit_choice_of_victim end\n");
}


