#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Estruturas de dados
typedef struct {
    int valid;        // Bit que indica se a linha é válida
    unsigned int tag; // Tag para identificar os dados
    int load_time;    // Para política FIFO
} CacheLine;

typedef struct {
    CacheLine *lines; // Linhas associadas ao conjunto
    int num_lines;    // Número de linhas por conjunto (associatividade)
} CacheSet;

typedef struct {
    CacheSet *sets;    // Conjuntos na cache
    int num_sets;      // Número total de conjuntos
    int block_size;    // Tamanho do bloco em bytes
    int assoc;         // Grau de associatividade
    char replacement_policy; // Política de substituição ('R', 'F', 'L')
    
    // Estatísticas
    struct {
        int hits;
        int misses;
        int compulsory;
        int conflict;
        int capacity;
        int totalAccesses;
    } stats;
} Cache;

// Funções
Cache* init_cache(int num_sets, int block_size, int assoc, char replacement_policy) {
    Cache* cache = (Cache*) malloc(sizeof(Cache));
    cache->num_sets = num_sets;
    cache->block_size = block_size;
    cache->assoc = assoc;
    cache->replacement_policy = replacement_policy;
    cache->sets = (CacheSet*) malloc(num_sets * sizeof(CacheSet));
    cache->stats.hits = 0;
    cache->stats.misses = 0;
    cache->stats.compulsory = 0;
    cache->stats.conflict = 0;
    cache->stats.capacity = 0;
    cache->stats.totalAccesses = 0;

    for (int i = 0; i < num_sets; i++) {
        cache->sets[i].lines = (CacheLine*) malloc(assoc * sizeof(CacheLine));
        for (int j = 0; j < assoc; j++) {
            cache->sets[i].lines[j].valid = 0;
        }
    }
    return cache;
}

int cacheIsFull(Cache *cache) {
    for (int i = 0; i < cache->num_sets; i++) {
        for (int j = 0; j < cache->assoc; j++) {
            if (!cache->sets[i].lines[j].valid) {
                return 0;  // Não está cheia
            }
        }
    }
    return 1;  // Está cheia
}

int access_cache(Cache *cache, unsigned int address, int *hit, int *miss, int *miss_comp, int *miss_conf, int *miss_cap) {
    unsigned int block_offset = address % cache->block_size;
    unsigned int index = (address / cache->block_size) % cache->num_sets;
    unsigned int tag = (address / cache->block_size) / cache->num_sets;

    CacheSet *set = &cache->sets[index];
    int empty_slot = -1;

    // Verifica por hits
    int hit_found = 0;  // Variável para verificar se foi encontrado um hit
    for (int i = 0; i < cache->assoc; i++) {
       if (set->lines[i].valid && set->lines[i].tag == tag) {
           hit_found = 1;  // Marca que encontramos um hit
           break;  // Sai do loop
       }
       if (!set->lines[i].valid && empty_slot == -1) {
           empty_slot = i;  // Marca um slot vazio
       }
    }
    if (hit_found) {
        (*hit)++;  // Incrementa o hit se encontrado
        return 1;  // Cache hit
    }

    // Caso contrário, é um miss
    *miss += 1;

    int line_to_replace;
    if (empty_slot != -1) {
        // Se houver slot vazio, é um miss compulsório
        line_to_replace = empty_slot;
        int *temp_ptr = miss_comp;  // Ponteiro temporário
        (*temp_ptr)++;  // Incrementa o valor
    } else {
        // Não há slots vazios, determinar tipo de miss
        line_to_replace = replace_line(cache, index, tag);  // Chama a função de substituição

        if (cacheIsFull(cache)) {
              // Se a cache estiver cheia, é um miss de capacidade
              int cap_temp = *miss_cap;  // Variável temporária para miss de capacidade
              cap_temp += 1;
              *miss_cap = cap_temp;
}       else {
          // Caso contrário, é um miss de conflito
          int conf_temp = *miss_conf;  // Variável temporária para miss de conflito
          conf_temp += 1;
          *miss_conf = conf_temp;
}
    }

    // Substitui a linha
    set->lines[line_to_replace].valid = 1;
    set->lines[line_to_replace].tag = tag;
    set->lines[line_to_replace].load_time = clock();  // Atualiza o tempo de carregamento

    return 0;  // Cache miss
}

int replace_line(Cache *cache, int index, unsigned int tag) {
    CacheSet *set = &cache->sets[index];
    int line_to_replace = 0;

    // Implementar política de substituição
    if (cache->replacement_policy == 'R') {
        // Randômica
        line_to_replace = rand() % cache->assoc;
    }

    // Substitui a linha
    set->lines[line_to_replace].valid = 1;
    set->lines[line_to_replace].tag = tag;
    set->lines[line_to_replace].load_time = clock();  // Atualiza o tempo de carregamento

    return line_to_replace;  // Retorna o índice da linha substituída
}

void read_address_file(const char *filename, Cache *cache) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Erro ao abrir o arquivo de entrada.\n");
        exit(1);
    }

    unsigned int address;
    int hit = 0, miss = 0, miss_comp = 0, miss_conf = 0, miss_cap = 0;
    int total_accesses = 0;

    while (fread(&address, sizeof(unsigned int), 1, file)) {
        address = __builtin_bswap32(address);  // Converter big endian para little endian
        access_cache(cache, address, &hit, &miss, &miss_comp, &miss_conf, &miss_cap);
        total_accesses++;
    }

    fclose(file);
    print_stats(total_accesses, hit, miss, miss_comp, miss_conf, miss_cap, 1);
}

void print_stats(int total_accesses, int hit, int miss, int miss_comp, int miss_conf, int miss_cap, int flagOut) {
    float hit_rate = (float) hit / total_accesses;
    float miss_rate = (float) miss / total_accesses;
    float comp_rate = (float) miss_comp / miss;
    float conf_rate = (float) miss_conf / miss;
    float cap_rate = (float) miss_cap / miss;

    if (flagOut == 0) {
        printf("Total de acessos: %d\n", total_accesses);
        printf("Taxa de hit: %.2f%%\n", hit_rate * 100);
        printf("Taxa de miss: %.2f%%\n", miss_rate * 100);
        printf("Misses compulsórios: %.2f%%\n", comp_rate * 100);
        printf("Misses de capacidade: %.2f%%\n", cap_rate * 100);
        printf("Misses de conflito: %.2f%%\n", conf_rate * 100);
    } else {
        // Formato padrão ajustado
        printf("%d, %.2f, %.2f, %.2f, %.2f, %.2f\n", total_accesses, hit_rate, miss_rate, comp_rate, cap_rate, conf_rate);
    }
}


int main(int argc, char *argv[]) {
    if (argc != 7) {
        printf("Numero de argumentos incorreto. Utilize:\n");
        printf("./cache_simulator <nsets> <bsize> <assoc> <substituição> <flag_saida> arquivo_de_entrada\n");
        exit(EXIT_FAILURE);
    }

    int nsets = atoi(argv[1]);
    int bsize = atoi(argv[2]);
    int assoc = atoi(argv[3]);
    char subst = argv[4][0];  // Considera que apenas uma letra foi passada
    int flagOut = atoi(argv[5]);
    char *arquivoEntrada = argv[6];

    printf("nsets = %d\n", nsets);
    printf("bsize = %d\n", bsize);
    printf("assoc = %d\n", assoc);
    printf("subst = %c\n", subst);
    printf("flagOut = %d\n", flagOut);
    printf("arquivo = %s\n", arquivoEntrada);

    srand(time(NULL));  // Inicializa a semente para substituição randômica

    // Inicializa a cache
    Cache *cache = init_cache(nsets, bsize, assoc, subst);

    // Simula os acessos à cache
    read_address_file(arquivoEntrada, cache);

    // Libera a memória da cache
    for (int i = 0; i < nsets; i++) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);

    return 0;
}
