#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fat.h>

int g_fat_loaded = 0;

static void
zero_data_cluster(union data_cluster* dc)
{
	int i;
	for (i = 0; i < BLOCK_SIZE / sizeof(dir_entry_t); i++)
		dc->dir[i].attributes = 0;
}

static void
update_fat(void)
{
	ptr_myfat = fopen("fat.part", "r+b");

	if (!ptr_myfat)
	{
		printf("Couldn't open fat!\n");
	}
	else
	{
		fseek(ptr_myfat, 1024, SEEK_SET);
		fwrite(&fat, sizeof(fat), 1, ptr_myfat);
	}

	fclose(ptr_myfat);
}

static int
save_data(int address, union data_cluster file)
{
	ptr_myfat = fopen("fat.part", "r+b");
	fseek(ptr_myfat, address * BLOCK_SIZE, SEEK_SET);
	fwrite(&file, sizeof(file), 1, ptr_myfat);
	fclose(ptr_myfat);
	return 0;
}

static union data_cluster*
load_cluster(int address)
{
	ptr_myfat = fopen("fat.part", "rb");
	fseek(ptr_myfat, address * BLOCK_SIZE, SEEK_SET);
	union data_cluster* cluster = malloc(sizeof(union data_cluster));
	fread(cluster, BLOCK_SIZE, 1, ptr_myfat);
	fclose(ptr_myfat);
	return cluster;
}

static void
set_fat_address(int address, int value)
{
	fat[address] = value;
	update_fat();
}

static int
get_free_address(void)
{
	int i;
	for (i = 10; i < FAT_SIZE; i++)
	{
		if (fat[i] == 0)
		{
			return i;
		}
	}
	return -1;
}

static int
check_directory_entry(const char* path, union data_cluster* cluster)
{
	int i;
	int flag = 0;
	for (i = 0; i < BLOCK_SIZE / sizeof(dir_entry_t); i++)
	{
		if (cluster->dir[i].attributes == 1 || cluster->dir[i].attributes == 2)
			if (strcmp((const char*)cluster->dir[i].filename, path) == 0)
				return 0;

		if (cluster->dir[i].attributes != 1 && cluster->dir[i].attributes != 2)
			flag = 1;
	}
	if (flag == 0)
		return 0;
	return 1;
}

static int
load_address_from_path(char** path, int size, int address)
{
	if (size == 1) {
		return address;
	}
	union data_cluster* cluster = load_cluster(address);
	int i;
	for (i = 0; i < BLOCK_SIZE / sizeof(dir_entry_t); i++)
	{
		if (strcmp((const char*)cluster->dir[i].filename, (const char*) *path) == 0)
		{
			int aux = load_address_from_path(++path, size - 1, cluster->dir[i].first_block);
			free(cluster);
			return aux;
		}
	}
	free(cluster);
	return -1;
}

int
init(void)
{
	int i;

	ptr_myfat = fopen("fat.part","wb");

	if (!ptr_myfat)
		return 1;


	//Initialize boot_block
	for (i = 0; i < BLOCK_SIZE; i++)
	{
		boot_block[i] = 0xbb;
	}

	//Write boot_block
	fwrite(&boot_block, sizeof(boot_block), 1, ptr_myfat);


	//Initialize fat
	fat[0] = 0xfffd;
	for (i = 1; i < 9; i++)
	{
		fat[i] = 0xfffe;
	}
	fat[9] = 0xffff;
	for (i = 10; i < FAT_SIZE; i++)
	{
		fat[i] = 0x0000;
	}

	g_fat_loaded = 1;

	//Write fat
	fwrite(&fat, sizeof(fat), 1, ptr_myfat);


	//Initialize root_dir
	for (i = 0; i < BLOCK_SIZE; i++)
	{
		root_dir[i] = 0x00;
	}

	//Write root_dir
	fwrite(&root_dir, sizeof(root_dir), 1, ptr_myfat);


	//Data clusters
	for (i = 0; i < DATA_CLUSTERS; i++)
	{
		fwrite(&cluster, sizeof(cluster), 1, ptr_myfat);
	}


	fclose(ptr_myfat);

	return 0;
}

int
create(char** path, int size)
{
	// Load addresses
	int address = get_free_address();

	// Load cluster address
	int cluster_address;
	if ((cluster_address = load_address_from_path(path + 1, size - 1, ROOT_ADDRESS)) == -1)
		return 1;

	// Load cluster
	union data_cluster* cluster = load_cluster(cluster_address);

	// Check directory
	if (!check_directory_entry(path[size - 1], cluster))
		return 1;


	// Write directory
	int i;
	for (i = 0; i < BLOCK_SIZE / sizeof(dir_entry_t); i++)
	{
		if (cluster->dir[i].attributes != 1 && cluster->dir[i].attributes != 2)
		{
			int j;
			for(j = 0; j < strlen(path[size - 1]) + 1; j++)
			{
				cluster->dir[i].filename[j] = path[size - 1][j];
			}
			cluster->dir[i].attributes = 2;
			cluster->dir[i].first_block = address;
			cluster->dir[i].size = 0;
			set_fat_address(address, -1);
			break;
		}

	}
	save_data(cluster_address, *cluster);

	free(cluster);

	return 0;
}

int
load(void)
{
	ptr_myfat = fopen("fat.part", "rb");

	if (!ptr_myfat)
	{
		return 1;
	}

	fseek(ptr_myfat, 1024, SEEK_SET);
	fread(&fat, sizeof(fat), 1, ptr_myfat);

	fclose(ptr_myfat);

	g_fat_loaded = 1;

	return 0;
}

int
is_empty(union data_cluster* cluster)
{
	int i;
	for (i = 0; i < BLOCK_SIZE / sizeof(dir_entry_t); i++)
		if (cluster->dir[i].attributes == 1 || cluster->dir[i].attributes == 2)
			return 0;
	return 1;
}

int
mkdir(char** path, int size)
{

	// Load addresses
	int address = get_free_address();

	// Load cluster address
	int cluster_address;
	if ((cluster_address = load_address_from_path(path + 1, size - 1, ROOT_ADDRESS)) == -1)
		return 1;

	// Load cluster
	union data_cluster* cluster = load_cluster(cluster_address);

	// Check directory
	if (!check_directory_entry(path[size - 1], cluster))
		return 1;

	// Write directory
	int i;
	for (i = 0; i < BLOCK_SIZE / sizeof(dir_entry_t); i++)
	{
		if (cluster->dir[i].attributes != 1 && cluster->dir[i].attributes != 2)
		{
			int j;
			for(j = 0; j < strlen(path[size - 1]) + 1; j++)
			{
				cluster->dir[i].filename[j] = path[size - 1][j];
			}
			cluster->dir[i].attributes = 1;
			cluster->dir[i].first_block = address;
			cluster->dir[i].size = 0;
			set_fat_address(address, -1);
			break;
		}

	}
	save_data(cluster_address, *cluster);

	// Create and save file
	union data_cluster new_file;
	zero_data_cluster(&new_file);
	save_data(address, new_file);

	free(cluster);

	return 0;
}

int
ls(char** path, int size)
{
	// Loads cluster
	int address = load_address_from_path(path + 1, size, ROOT_ADDRESS);
	union data_cluster* cluster = load_cluster(address);

	// Prints All Filenames
	int i;
	for (i = 0; i < BLOCK_SIZE / sizeof(dir_entry_t); i++)
	{
		if (cluster->dir[i].attributes == 1)
			printf("\t%4dB dir  %s\n", cluster->dir[i].size, cluster->dir[i].filename);

		if (cluster->dir[i].attributes == 2)
			printf("\t%4dB file %s\n", cluster->dir[i].size, cluster->dir[i].filename);
	}

	free(cluster);

	return 0;

}

int
unlink_fat(int address)
{
	if (fat[address] == 0xffff)
	{
		set_fat_address(address, 0);
		return 1;
	}
	int next_hop = fat[address];
	fat[address] = 0;
	return unlink_fat(next_hop);

}

int
unlink(char** path, int size)
{
	// Load previous address
	int prev_address = load_address_from_path(path + 1, size - 1, ROOT_ADDRESS);
	union data_cluster* prev_cluster = load_cluster(prev_address);

	int i;
	for (i = 0; i < BLOCK_SIZE / sizeof(dir_entry_t); i++)
	{
		if (strcmp((const char*)prev_cluster->dir[i].filename, (const char*)path[size - 1]) == 0)
		{
			if (prev_cluster->dir[i].attributes == 1)
			{
				int address = prev_cluster->dir[i].first_block;
				union data_cluster* cluster = load_cluster(address);
				if(is_empty(cluster))
				{
					prev_cluster->dir[i].attributes = 0;
					unlink_fat(address);
				}
				free(cluster);
			}
			if (prev_cluster->dir[i].attributes == 2)
			{
				int address = prev_cluster->dir[i].first_block;
				prev_cluster->dir[i].attributes = 0;
				unlink_fat(address);
			}
		}
	}
	save_data(prev_address, *prev_cluster);

	free(prev_cluster);

	return 0;
}

int
write(char** path, int size, char* string)
{
	int address = load_address_from_path(path + 1, size, ROOT_ADDRESS);
	union data_cluster* cluster = load_cluster(address);

	int i;
	int k = 0;
	for (i = 0; i < strlen(string) + 1; i++)
	{
		cluster->data[k++] = string[i];
		if (k == BLOCK_SIZE)
		{
			save_data(address, *cluster);
			int next_addr = get_free_address();
			set_fat_address(address, next_addr);
			set_fat_address(next_addr, 0xffff);
			address = next_addr;
			free(cluster);
			cluster = load_cluster(address);
			k = 0;
		}
	}
	set_fat_address(address, 0xffff);

	save_data(address, *cluster);

	free(cluster);

	int parent_addr = load_address_from_path(path + 1, size - 1, ROOT_ADDRESS);
	union data_cluster* parent = load_cluster(parent_addr);

	for (i = 0; i < BLOCK_SIZE / sizeof(dir_entry_t); i++)
	{
		if ( strcmp((const char*)parent->dir[i].filename, path[size - 1]) == 0 )
			parent->dir[i].size = strlen(string) * sizeof(char) + 1;
	}
	save_data(parent_addr, *parent);

	free(parent);

	return 0;
}

int
append(char** path, int size, char* string)
{
	int address = load_address_from_path(path + 1, size, ROOT_ADDRESS);

	while (fat[address] != 0xffff)
	{
		address = fat[address];
	}

	union data_cluster* cluster = load_cluster(address);

	int eof;
	for (eof = 0; eof < BLOCK_SIZE + 1; eof++)
		if (cluster->data[eof] == 0)
			break;

	int i;
	for (i = 0; i < strlen(string) + 1; i++)
	{
		cluster->data[eof++] = string[i];
		if (eof == BLOCK_SIZE)
		{
			save_data(address, *cluster);
			int next_addr = get_free_address();
			set_fat_address(address, next_addr);
			set_fat_address(next_addr, 0xffff);
			address = next_addr;
			free(cluster);
			cluster = load_cluster(address);
			eof = 0;
		}
	}

	save_data(address, *cluster);

	free(cluster);

	int parent_addr = load_address_from_path(path + 1, size - 1, ROOT_ADDRESS);
	union data_cluster* parent = load_cluster(parent_addr);

	for (i = 0; i < BLOCK_SIZE / sizeof(dir_entry_t); i++)
	{
		if ( strcmp((const char*)parent->dir[i].filename, path[size - 1]) == 0 )
			parent->dir[i].size = parent->dir[i].size + strlen(string) * sizeof(char) + 1;
	}
	save_data(parent_addr, *parent);

	free(parent);

	return 0;
}

int
read(char** path, int size)
{
	int address = load_address_from_path(path + 1, size, ROOT_ADDRESS);
	union data_cluster* cluster = load_cluster(address);

	while (fat[address] != 0xffff)
	{
		printf("%s", cluster->data);
		address = fat[address];
		free (cluster);
		cluster = load_cluster(address);
	}
	printf("%s\n", cluster->data);

	free(cluster);

	return 0;
}
