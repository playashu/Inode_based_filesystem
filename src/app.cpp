#include<iostream>
#include<vector>
#include<unordered_map>
#include<map>
#include <string>
#include <unistd.h>

using namespace std;

struct Inode{
	char name[32];
	uint32_t size;
	uint32_t block[5];
	uint32_t indirect_block_pointer;

	Inode(){
	}

	Inode(string n){
		strcpy(name,n.substr(0,31).c_str());
		size=0;
		for (int i = 0; i < 5; i++){
		    block[i] = 0;
		}
		indirect_block_pointer=0;
	}

};
const uint32_t count_direct_pointer=5;
const uint32_t block_size = 4096;
const uint32_t block_count = 131072;
const uint32_t inode_start = 0;
const uint32_t inode_blocks_count = 4;
const uint32_t data_start = inode_start + inode_blocks_count;
const uint32_t max_files_count = ((inode_blocks_count * block_size)-4) / sizeof(struct Inode);
const uint32_t block_store_size = block_size/4 ;
string d_name;
map<int, string> openFiles;
int i=1;
string CURR_DIR;

struct Drive * mounted_drive = NULL;

struct Drive{
	FILE * fd;
	unordered_map<string, struct Inode *> files;
	vector<bool > free_blocks;
	Drive(FILE * a){
		fd = a;
		for (int i = 0; i<block_count ; i++){
			free_blocks.push_back(true);
		}
		for(int i =0 ; i< data_start ;i++ ){
			free_blocks[i]=false;
		}
	}
	void freeMap(int n){
		free_blocks[n] = true;
	}

	
	~Drive(){
		fclose(fd);
	}
};

struct IndirectBlock{
	uint32_t block[block_store_size];

	IndirectBlock(){
		for(int i = 0; i <block_store_size ;i++){
			block[i]=0;
		}
	}
	int insert(uint32_t n){
		int i;
		for(i = 0; i <block_store_size ;i++){
			if(block[i]!=0){
				block[i]=n;
				break;
			}
		}
		if(i==block_store_size){
			return -1;
		}
		return 0;
	}
};

uint32_t min(uint32_t x, uint32_t y){
  return (x < y) ? x : y ;
}
string getPresentDirectory(){
    char cwd[500];
    getcwd(cwd, 500);
    return cwd;
}

void create_disk(string name){
	char buff[block_size];
	string path = CURR_DIR + "/"+name+".dat";
	FILE *fd;
	if(fd = fopen(path.c_str(),"r")){
		fclose(fd);
		cout<<"Disk With the name already exists!"<<endl;
	}else{
		fd = fopen(path.c_str(), "w+");
		
		for(uint32_t i = 0; i < block_size; i++){
	        buff[i] = 0;
	    }
	    for(uint32_t i = 0; i < block_count; i++){
	        fwrite (buff, block_size, 1, fd);
	    }
	    cout<<"Disk Created!"<<endl;
	    fclose(fd);
	}
		
}

int mount_disk(string name){
	
	uint32_t count;

	string path = CURR_DIR + "/"+name+".dat";
	
	FILE *fd = fopen(path.c_str(), "r+");
	
	if(fd){
		cout<<"Disk Mounted"<<endl;
	}else{
		cout<<"Disk doesn't exists"<<endl;
		return -1;
	}

	fseek(fd , inode_start * block_size, SEEK_SET);

	fread(&count, sizeof(uint32_t), 1, fd);

	//cout<<"number of file present ::"<<count<<endl;
	
	mounted_drive = new Drive(fd);
	
	for(int i = 0;i<count;i++){
		struct Inode * inode = new Inode();
		fread(inode, sizeof(struct Inode), 1, fd);
		mounted_drive->files.insert(make_pair(inode->name,inode));
		//cout<<"reading inode from file, index : "<<mounted_drive->files.size()<<endl;
	}	

	for (auto i : mounted_drive->files){
		//cout<<i.second.name;
		for(auto j : i.second->block){
			if(j!=0){
				//cout<<"::"<<j;
				mounted_drive->free_blocks[j]=false;
			}
		}
		struct IndirectBlock iB;
		uint32_t n = i.second->indirect_block_pointer;
		if(n!=0){
			mounted_drive->free_blocks[n]=false;
			FILE *fd = mounted_drive->fd;
			fseek(fd, n * block_size, SEEK_SET);
			if(fread(&iB, sizeof(struct IndirectBlock), 1, fd)){
				for(int k=0;k<block_store_size;k++){
					if(iB.block[k]!=0){
						mounted_drive->free_blocks[iB.block[k]]=false;
					}
				}
			}else{
				cout<<"something probably wrong"<<endl;
			}
		}

	}
	return 0;
}
bool search_map(map<int, string> &my_map ,string &value){
	map<int, string>::iterator it;
	for(it = my_map.begin(); it != my_map.end(); ++it ){
      if(value == it->second){
      		return true;
      };
    }
    return false;
}

void update_header(){
	string path = CURR_DIR + "/"+d_name+".dat";
	uint32_t size =  0;
	FILE * fd = mounted_drive->fd;

	uint32_t files_count=mounted_drive->files.size();
	//cout<<files_count<<endl;
	fseek(fd , 0, SEEK_SET);
	fwrite (&files_count, sizeof(uint32_t), 1, fd);
	
	for (auto i : mounted_drive->files){
		// cout<<i.second->name<<" "<<i.second->size<<endl;
		// for(auto k : i.second->block){
		// 	cout<<"::"<<k;
		// }
		fwrite (i.second, sizeof(struct Inode), 1, mounted_drive->fd);
	}
	
	size = data_start* block_size - ftell(mounted_drive->fd);
		
	char zero = 0;
    for (int i = 0; i < size; ++i){ 
	    fwrite(&zero, sizeof(char), 1, mounted_drive->fd);
	}
	//other way around is to 1st fill with zero then with inode 
}

void unmount_disk(string name){
	update_header();
	openFiles.clear();
	i=0;
	delete(mounted_drive);
	mounted_drive = NULL;
}

int create_file(string file_name){
	if(mounted_drive->files.find(file_name)==mounted_drive->files.end()){
		if(mounted_drive->files.size()<max_files_count){

			Inode * n = new Inode(file_name);
			mounted_drive->files.insert(make_pair(file_name,n));
			cout<<"File Added"<<endl;
			update_header();
		}else{
			cout<<"Max File limit reached"<<endl;
			return -1;
		}
	}else{
		cout<<"File with the name already exists"<<endl;
		return -1;
	}
	return 0;
}

void write_to_file(string file_name, int mode, string input){
	//somewhat only asssuming file doesnt exists!

	long long remaining_size = input.size();
	long long orignal_size = input.size();
	if(mounted_drive->files.find(file_name)==mounted_drive->files.end()){
		cout<<"No File exists!"<<endl;
		return;
	}
	if(mode==1){
		uint32_t present_size = mounted_drive->files[file_name]->size;
		uint32_t blocks_occp = (present_size>0)?floor(present_size/block_size)+1:0;
	
		uint32_t req_size = input.size();
		uint32_t blocks_req = (req_size>0)?floor(req_size/block_size)+1:0;
		
		if(blocks_req>count_direct_pointer && blocks_occp<count_direct_pointer){
			blocks_req += 1;   //one extra required for indirect one
			//cout<<"*************-*-*-*-*-*-*-*-*-*-*--*-*-************************"<<endl;
		}

		uint32_t block_diff = blocks_req - blocks_occp;
		vector<uint32_t> freeB;
		cout<<"# Old Size = "<<present_size<<endl;
		cout<<"# Blocks Occupied = "<<blocks_occp<<endl;
		cout<<"# Blocks Req. = "<<blocks_req<<endl;
 		for(int i=0;i <block_count && freeB.size()<blocks_req +2;i++){
			if(mounted_drive->free_blocks[i]==true){
				freeB.push_back(i);
			}
		}

		if(freeB.size()<block_diff){
			cout<<"Not Enough space to accomodate the input"<<endl;
		}else{
			int j=0;
			int i=0;
			uint32_t n;
			while(i<count_direct_pointer){
				if(remaining_size!=0){
					n = mounted_drive->files[file_name]->block[i];
					if(n==0){
						mounted_drive->files[file_name]->block[i]=freeB[j];
						n=freeB[j];
						mounted_drive->free_blocks[freeB[j]]=false;
						j++;
					}
					//cout<<freeB[j];
					
					fseek(mounted_drive->fd, n*block_size, SEEK_SET);
					
					uint32_t wrote = min(remaining_size, block_size);
					remaining_size -= wrote;
					fwrite(&input[i*block_size], wrote, 1, mounted_drive->fd);
				}else{
					uint32_t n = mounted_drive->files[file_name]->block[i];
					mounted_drive->files[file_name]->block[i] = 0;
					mounted_drive->free_blocks[n]=true;
				}
				i++;
			}
			if(remaining_size!=0){
				IndirectBlock iB;
				uint32_t n = mounted_drive->files[file_name]->indirect_block_pointer;
				if(n==0){			//create a indirect pointer
					mounted_drive->files[file_name]->indirect_block_pointer=freeB[j];
					mounted_drive->free_blocks[freeB[j++]]=false;
					//fseek(mounted_drive->fd, freeB[j++]*block_size, SEEK_SET);
					//fwrite (&iB, sizeof(struct IndirectBlock), 1, mounted_drive->fd);
				}else{
					fseek(mounted_drive->fd, n*block_size, SEEK_SET);
					fread(&iB, sizeof(struct IndirectBlock), 1, mounted_drive->fd);
				}
				
				//fread(&iB, sizeof(struct IndirectBlock), 1, mounted_drive->fd);
				int k=0;
				while(remaining_size!=0){
					iB.block[k]=freeB[j];
					mounted_drive->free_blocks[freeB[j]]=false;
					fseek(mounted_drive->fd, freeB[j++]*block_size, SEEK_SET);
					uint32_t wrote = min(remaining_size, block_size);
					remaining_size -= wrote;
					fwrite(&input[i*block_size], wrote, 1, mounted_drive->fd);
					i++;
					k++;
					//allocate from indirect pointer
				}
				uint32_t iB_index = mounted_drive->files[file_name]->indirect_block_pointer;
				fseek(mounted_drive->fd, iB_index * block_size, SEEK_SET);
				fwrite (&iB, sizeof(struct IndirectBlock), 1, mounted_drive->fd);

				// cout<<"********************************"<<endl;
				// for(auto i : iB.block){
				// 	cout<<i<<"|";
				// }
			}else{
				uint32_t n = mounted_drive->files[file_name]->indirect_block_pointer;
				mounted_drive->files[file_name]->indirect_block_pointer=0;
				mounted_drive->free_blocks[n]=true;
			}
			mounted_drive->files[file_name]->size=input.size();
			cout<<"# Bytes Written : "<<input.size()<<endl;
			cout<<"# New Size : "<<input.size()<<endl;
			//cout<<"File written!!  value of i ::"<<i<<endl;
		}
			
	}
	if(mode==2){
		uint32_t present_size = mounted_drive->files[file_name]->size;
		uint32_t blocks_occp = (present_size>0)?floor(present_size/block_size)+1:0;
	
		uint32_t req_size = input.size();
		uint32_t blocks_req = (req_size>0)?floor(req_size/block_size)+1:0;
		
		if(blocks_req + blocks_occp >count_direct_pointer && blocks_occp<count_direct_pointer){
			blocks_req += 1;   //one extra required for indirect one
		}

		//uint32_t block_diff = blocks_req - blocks_occp;
		vector<uint32_t> freeB;
		cout<<"# Present Size = "<<present_size<<endl;
		cout<<"# Blocks Occupied = "<<blocks_occp<<endl;
		cout<<"# Blocks Req. = "<<blocks_req<<endl;
 		
 		for(int i=0;i <block_count && freeB.size()<blocks_req +2;i++){
			if(mounted_drive->free_blocks[i]==true){
				freeB.push_back(i);
			}
		}
		
		if(freeB.size()<blocks_req){
			cout<<"Not Enough space to accomodate the input"<<endl;
		}else{
			int j=0;
			int i=0;
			int w=0;
			uint32_t n;
			while(i<count_direct_pointer && mounted_drive->files[file_name]->block[i]!=0){
				i++;
			}
			
			if(i<count_direct_pointer 
				|| ( i==count_direct_pointer 
						&& mounted_drive->files[file_name]->indirect_block_pointer==0) ){
			
									//reading the last partially filled block
				
				if(i>0){
					i--;

					uint32_t read_chunk = present_size - (blocks_occp-1) * block_size;
					n = mounted_drive->files[file_name]->block[i];
					char temp[read_chunk];
					fseek(mounted_drive->fd, n * block_size, SEEK_SET);
					fread(&temp[0], read_chunk, 1 , mounted_drive->fd);

										//updating the input and size
					input= temp + input;
					remaining_size=input.size();
				}else{
					n = freeB[j];
					mounted_drive->files[file_name]->block[i]=n;
					mounted_drive->free_blocks[n]=false;
					j++;
				}

				while(i<count_direct_pointer){
					if(remaining_size!=0){	
						fseek(mounted_drive->fd, n*block_size, SEEK_SET);
						uint32_t wrote = min(remaining_size, block_size);
						remaining_size -= wrote;
						fwrite(&input[w*block_size], sizeof(char), wrote, mounted_drive->fd);
						//mounted_drive->files[file_name]->size+=wrote;
						w++;
						i++;
						if(remaining_size!=0 && j<freeB.size()){
							mounted_drive->files[file_name]->block[i]=freeB[j];	
							n = freeB[j];
							mounted_drive->free_blocks[freeB[j]]=false;
							j++;
						}
						//cout<<freeB[j];
					}else{
						n = mounted_drive->files[file_name]->block[i];
						mounted_drive->files[file_name]->block[i] = 0;
						mounted_drive->free_blocks[n]=true;
						i++;
					}
					
				}
			}

			
			if(remaining_size!=0){
				IndirectBlock iB;
				int k=0;
				uint32_t n = mounted_drive->files[file_name]->indirect_block_pointer;
				if(n==0){			
										//create a indirect pointer
					
					mounted_drive->files[file_name]->indirect_block_pointer=freeB[j];
					mounted_drive->free_blocks[freeB[j]]=false;
					j++;
					//fseek(mounted_drive->fd, freeB[j++]*block_size, SEEK_SET);
					//fwrite (&iB, sizeof(struct IndirectBlock), 1, mounted_drive->fd);
					//cout<<"sdadsadasdasdsadadsadadasdsada**********************"<<endl;
					
					iB.block[k]=freeB[j];
					mounted_drive->free_blocks[freeB[j]]=false;
					j++;
				
				}else{
					
					fseek(mounted_drive->fd, n*block_size, SEEK_SET);
					fread(&iB, sizeof(struct IndirectBlock), 1, mounted_drive->fd);
					
					while(iB.block[k]!=0){
						k++;
					}
					

										//reading the partially filled last block (i.e. k-1)
					k--;
					uint32_t read_chunk;
					if(blocks_occp > count_direct_pointer){
					 	read_chunk = present_size - (blocks_occp-2) * block_size;
					}else{
						read_chunk = present_size - (blocks_occp-1) * block_size;
					}
					char temp[read_chunk];
					fseek(mounted_drive->fd, iB.block[k] * block_size, SEEK_SET);
					fread(&temp[0], read_chunk, 1 , mounted_drive->fd);
					
					input= temp + input;
					remaining_size = input.size();
				}
				
				
				//fread(&iB, sizeof(struct IndirectBlock), 1, mounted_drive->fd);

				while(remaining_size!=0){
					
					fseek(mounted_drive->fd, iB.block[k]*block_size, SEEK_SET);
					uint32_t wrote = min(remaining_size, block_size);
					remaining_size -= wrote;
					fwrite(&input[w*block_size], wrote, 1, mounted_drive->fd);
					w++;
					k++;
					
					if(remaining_size!=0 && j<freeB.size()){
						iB.block[k]=freeB[j];
						mounted_drive->free_blocks[freeB[j]]=false;
						j++;
					}
					
				}

								//writning the updated indirect block to disk!!
				
				uint32_t iB_index = mounted_drive->files[file_name]->indirect_block_pointer;
				fseek(mounted_drive->fd, iB_index * block_size, SEEK_SET);
				fwrite (&iB, sizeof(struct IndirectBlock), 1, mounted_drive->fd);

				// cout<<"********************************"<<endl;
				// for(auto i : iB.block){
				// 	cout<<i<<"|";
				// }
			}else{

				uint32_t n = mounted_drive->files[file_name]->indirect_block_pointer;
				mounted_drive->files[file_name]->indirect_block_pointer=0;
				mounted_drive->free_blocks[n]=true;
			}
			//mounted_drive->files[file_name]->size+=input.size();
			mounted_drive->files[file_name]->size+=orignal_size;
			cout<<"# Bytes Written ::"<<orignal_size<<endl;
			cout<<"# New Size ::"<<mounted_drive->files[file_name]->size<<endl;
		}
		
	}
	update_header();
}

void read_from_file(string file_name){
	//somewhat only asssuming file doesnt exists!
	string mode ="read";
	
	if(mounted_drive->files.find(file_name)==mounted_drive->files.end()){
		cout<<"No File exists!"<<endl;
		return;
	}
	if(true){
		uint32_t present_size = mounted_drive->files[file_name]->size;
		uint32_t blocks_occp = (present_size>0)?floor(present_size/block_size)+1:0;
	
		vector<uint32_t > blocks_to_read;
		Inode * iNode = mounted_drive->files[file_name];
		for(auto j : iNode->block){
			if(j!=0){
				blocks_to_read.push_back(j);
			}else{
				break;
			}
		}
		
		if(blocks_to_read.size()<blocks_occp){
			uint32_t n = iNode->indirect_block_pointer;
			if(n!=0){
				IndirectBlock iB;
				FILE * fd = mounted_drive->fd;
				fseek(fd, n * block_size, SEEK_SET);
				if(fread(&iB, sizeof(struct IndirectBlock), 1, fd)){
					
					for(int k=0;k<block_store_size;k++){
						if(iB.block[k]!=0){
							blocks_to_read.push_back(iB.block[k]);
						}
					}
				}else{
					cout<<"something probably wrong"<<endl;
				}
				// cout<<"Fdsll"<<endl;
				// cout<<"********************************"<<endl;
				// 	for(auto i : blocks_to_read){
				// 		cout<<i<<"|";
				// 	}
				// 	cout<<"************------****************"<<endl;
				if(blocks_to_read.size()==blocks_occp){
					//cout<<"Read successfull"<<endl;
				}else{
					//cout<<"File read failed!! |"<<blocks_to_read.size()<<" v/s "<<blocks_occp <<endl;
				}
			}else{
				//cout<<"indirect block pointer is zero"<<endl;
			}
		}else{
			//cout<<"File Read successfull"<<endl;
		}
		int i=0;
		uint32_t remaining_size = mounted_drive->files[file_name]->size;
		string content="";
		while(remaining_size!=0){
			uint32_t read_chunk = min(remaining_size,block_size);
			remaining_size -= read_chunk;
			char temp[read_chunk];
			fseek(mounted_drive->fd, blocks_to_read[i] * block_size, SEEK_SET);
			fread(&temp[0], read_chunk, 1 , mounted_drive->fd);
			content=content+temp;
			//cout<<endl<<endl<<endl<<endl<<":::::::::::::::::::::::::::"<<i<<":::::::::::::::::::::::"<<endl<<endl<<endl<<endl;
			//cout<<temp;
			i++;

		}	
		cout<<content;
	}

}

int open_file(string x){
	if(!search_map(openFiles,x)){
		if(mounted_drive->files.find(x)!=mounted_drive->files.end()){
			openFiles.insert(make_pair(i, x));
			i++;
			return i;
		}else{
			cout<<"No file exists"<<endl;
		}
	}else{
		cout<<"File Already opened!"<<endl;
	}
	return -1;
}

int close_file(int x){
	if(openFiles.find(x)!=openFiles.end()){
		cout<<"closed the file : "<<openFiles[x];
		openFiles.erase(x);
		return 0;
	}else{
		cout<<"file not opened!"<<endl;
	}
	return -1;
}

int delete_file(string file_name){
	if(!search_map(openFiles,file_name)){
		if(mounted_drive->files.find(file_name)!=mounted_drive->files.end()){
			mounted_drive->files.erase(file_name);
			update_header();
			cout<<"deleted "<<file_name<<endl;
			return 0;
		}else{
			cout<<"No file exists"<<endl;
		}
	}else{
		cout<<"Please Close the file 1st!"<<endl;
	}
	return -1;
}

void list_files(){
	if(!mounted_drive->files.empty()){
		for( auto it : mounted_drive->files){
		    cout<<"+ "<<it.first<<endl;
		}
	}else{
		cout<<"NO files present"<<endl;
	}
	
}

void list_open_files(){
	if(!openFiles.empty()){
		for(auto i : openFiles){
			cout<<"+ "<<i.first<<" :: "<<i.second<<endl;
		}
	}else{
		cout<<"NO files present"<<endl;
	}
	
}
void clearScreen(){
    cout << "\033c"; //clear screen
}

int main(){
	int n,x,m,c;
	char a;
	string s,input,disk_name;
	CURR_DIR = getPresentDirectory();
	//create_disk("sss");
	//mount_disk("sss");

	while(true){
		clearScreen();
		cout<<"================================================================"<<endl;
		cout<<"\n1. Create disk\n2. Mount Disk\n3. Exit\n"<<endl;
		cout<<"================================================================"<<endl;
		cout<<"Choose an option : \t\t\t\t\t";
		cin>>m;
		if(m==1){
			cout<<"Enter Disk Name : \t\t\t\t\t";
			cin>>disk_name;
			create_disk(disk_name);
			cout<<endl<<"PRESS ENTER TO CONTINUE....";
			system("read");
		}else if(m==2){
			cout<<"Enter Disk Name : \t\t\t\t\t";
			cin>>disk_name;
			if(mount_disk(disk_name)!=0){
				cout<<endl<<"PRESS ENTER TO CONTINUE....";
				system("read");
				continue;
			};
			d_name=disk_name;
			system("read");
			clearScreen();
			while(true){
				cout<<"================================================================"<<endl;
				cout<<"\n1. Create file\n2. Open file\n3. Read file\n4. Write File\n5. Append File\n6. Close File\n";
				cout<<"7. Delete File\n8. List Files\n9. List Open Files\n10. Unmount";
				cout<<"\n================================================================"<<endl;
				cout<<"Choose a option: \t\t\t\t\t";
				cin>>n;
				if(n==1){
					cout<<"Enter File Name : \t\t\t\t\t";
					cin>>s;
					create_file(s);

				}else if(n==2){
					cout<<"Enter file name to be opened : \t\t\t\t";
					cin>>s;
					if(!search_map(openFiles,s)){
						cout<<"File descriptor ::\t\t\t\t\t"<<open_file(s);
					}else{
						cout<<"File already open"<<endl;
					}
		
				}else if(n==3){
					cout<<"Enter file's descriptor to be read : \t\t\t";
					cin>>x;
					if(openFiles.find(x)!=openFiles.end()){
						cout<<endl<<"-*-*-*-*-"<<openFiles[x]<<"-*-*-*-*-"<<endl;
						read_from_file(openFiles[x]);
						cout<<endl<<"-*-*-*-*-"<<"END"<<"-*-*-*-*-"<<endl;
				
					}else{
						cout<<"File not open!"<<endl;
					}
				}else if(n==4){
					cout<<"Enter file's descriptor to be written : \t\t";
					cin>>x;
					if(openFiles.find(x)!=openFiles.end()){
						cout<<"Enter Input Text below::"<<endl;
						cout<<endl<<"-*-*-*-*-*-*-*-*-*-*-*-*-*-*-"<<endl;
						std::getline(std::cin >> std::ws, input);
						//cin>>input;
						write_to_file(openFiles[x],1,input);
					}else{
						cout<<"File not open!"<<endl;
					}
					
				}else if(n==5){
					cout<<"Enter file's descriptor to be appended : \t\t";
					cin>>x;
					if(openFiles.find(x)!=openFiles.end()){
						cout<<"Enter Input Text below::"<<endl;
						cout<<endl<<"-*-*-*-*-*-*-*-*-*-*-*-*-*-*-"<<endl;
						std::getline(std::cin >> std::ws, input);
						//cin>>input;
						write_to_file(openFiles[x],2,input);
					}else{
						cout<<"File not open!"<<endl;
					}
				}else if(n==6){
					cout<<"Enter file's descriptor to be closed : \t\t";
					cin>>x;
					close_file(x);
				}else if(n==7){
					cout<<"Enter file's name to be deleted : \t\t";
					cin>>s;
					delete_file(s);
				}else if(n==8){
					list_files();
				
				}else if(n==9){
					list_open_files();
				
				}else if(n==10){
					unmount_disk(disk_name);
					cout<<"\n================================================================"<<endl;
					break;
				}
				cout<<endl<<"PRESS ENTER TO CONTINUE....";
				system("read");
				clearScreen();
			}
		}else{
			clearScreen();
			exit(0);
		}
			
	}
	return 0;
}