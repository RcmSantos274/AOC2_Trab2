#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Contador global para simular o tempo
static unsigned long long global_time = 0;

// Definição de tipos de acesso
#define READ 0
#define WRITE 1

// Estrutura da linha de cache atualizada
typedef struct {
    int valid;
    unsigned int tag;
    unsigned long long last_used;
    unsigned long long load_time;
    unsigned int frequency;  // Nova: para LFU
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
    char replacement_policy; // Política de substituição ('R', 'F', 'L', 'U')
} Cache;

// Protótipos de funções
Cache* init_cache(int num_sets, int block_size, int assoc, char replacement_policy);
int access_cache(Cache *cache, unsigned int address, int access_type, int *hit, int *miss, int *miss_comp, int *miss_conf, int *miss_cap);
void replace_line(Cache *cache, int index, unsigned int tag);
void read_address_file(const char *filename, Cache *cache);
void print_stats(int total_accesses, int hit, int miss, int miss_comp, int miss_conf, int miss_cap, int flagOut);

// Função de inicialização da cache
Cache* init_cache(int num_sets, int block_size, int assoc, char replacement_policy) {
    Cache *cache = (Cache*) malloc(sizeof(Cache));
    if (cache == NULL) {
        printf("Erro: Falha na alocação de memória para a cache.\n");
        exit(1);
    }
    cache->num_sets = num_sets;
    cache->block_size = block_size;
    cache->assoc = assoc;
    cache->replacement_policy = replacement_policy;
    cache->sets = (CacheSet*) malloc(num_sets * sizeof(CacheSet));
    if (cache->sets == NULL) {
        printf("Erro: Falha na alocação de memória para os conjuntos da cache.\n");
        free(cache);
        exit(1);
    }

    for (int i = 0; i < num_sets; i++) {
        cache->sets[i].lines = (CacheLine*) malloc(assoc * sizeof(CacheLine));
        if (cache->sets[i].lines == NULL) {
            printf("Erro: Falha na alocação de memória para as linhas da cache.\n");
            for (int j = 0; j < i; j++) {
                free(cache->sets[j].lines);
            }
            free(cache->sets);
            free(cache);
            exit(1);
        }
        for (int j = 0; j < assoc; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].last_used = 0;
            cache->sets[i].lines[j].load_time = 0;
            cache->sets[i].lines[j].frequency = 0;  // Inicialização para LFU
        }
    }
    return cache;
}

// Função de simulação de acesso ao cache atualizada
int access_cache(Cache *cache, unsigned int address, int access_type, int *hit, int *miss, int *miss_comp, int *miss_conf, int *miss_cap) {
    unsigned int block_offset = address % cache->block_size;
    unsigned int index = (address / cache->block_size) % cache->num_sets;
    unsigned int tag = (address / cache->block_size) / cache->num_sets;

    CacheSet *set = &cache->sets[index];
    int found_free_line = 0;

    // Verifica por hits
    for (int i = 0; i < cache->assoc; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            *hit += 1;
            set->lines[i].last_used = global_time++;
            set->lines[i].frequency++;  // Atualiza frequência para LFU

            if (access_type == WRITE) {
                // Implementar lógica para escrita (write-through ou write-back)
            }
            
            return 1;  // Cache hit
        }
    }

    // Caso contrário, é um miss
    *miss += 1;

    // Verifica se o miss é compulsório
    for (int i = 0; i < cache->assoc; i++) {
        if (!set->lines[i].valid) {
            *miss_comp += 1;  // Contagem de miss compulsório
            found_free_line = 1;
            break;
        }
    }

    if (!found_free_line) {
        // Verifica se todos os conjuntos estão cheios
        int all_sets_full = 1;
        for (int i = 0; i < cache->num_sets; i++) {
            for (int j = 0; j < cache->assoc; j++) {
                if (!cache->sets[i].lines[j].valid) {
                    all_sets_full = 0;
                    break;
                }
            }
            if (!all_sets_full) break;
        }
        
        if (all_sets_full) {
            *miss_cap += 1;   // Miss de capacidade
        } else {
            *miss_conf += 1;  // Miss de conflito
        }
    }

    // Substituição de linha
    replace_line(cache, index, tag);

    if (access_type == WRITE) {
        // Implementar lógica para escrita (write-through ou write-back)
    }

    return 0;  // Cache miss
}

// Função para substituir linha no cache atualizada
void replace_line(Cache *cache, int index, unsigned int tag) {
    CacheSet *set = &cache->sets[index];
    int line_to_replace = 0;

    if (cache->replacement_policy == 'R') {
        // Randômica
        line_to_replace = rand() % cache->assoc;
    } else if (cache->replacement_policy == 'F') {
        // FIFO
        static int next_to_replace = 0;
        line_to_replace = next_to_replace;
        next_to_replace = (next_to_replace + 1) % cache->assoc;
    } else if (cache->replacement_policy == 'L') {
        // LRU
        unsigned long long lru = set->lines[0].last_used;
        for (int i = 1; i < cache->assoc; i++) {
            if (set->lines[i].last_used < lru) {
                lru = set->lines[i].last_used;
                line_to_replace = i;
            }
        }
    } else if (cache->replacement_policy == 'U') {
        // LFU
        unsigned int lfu = set->lines[0].frequency;
        for (int i = 1; i < cache->assoc; i++) {
            if (set->lines[i].frequency < lfu) {
                lfu = set->lines[i].frequency;
                line_to_replace = i;
            }
        }
    }

    // Substitui a linha
    set->lines[line_to_replace].valid = 1;
    set->lines[line_to_replace].tag = tag;
    set->lines[line_to_replace].load_time = global_time++;
    set->lines[line_to_replace].last_used = global_time;
    set->lines[line_to_replace].frequency = 1;  // Reset frequency for LFU
}

// Função para ler arquivo de endereços e simular acessos
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
        #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        address = __builtin_bswap32(address);
        #endif
        // Simula acesso de leitura. Para simular escrita, mude READ para WRITE
        access_cache(cache, address, READ, &hit, &miss, &miss_comp, &miss_conf, &miss_cap);
        total_accesses++;
    }

    fclose(file);
    print_stats(total_accesses, hit, miss, miss_comp, miss_conf, miss_cap, 1);
}

// Função para imprimir as estatísticas do cache
void print_stats(int total_accesses, int hit, int miss, int miss_comp, int miss_conf, int miss_cap, int flagOut) {
    float hit_rate = (float) hit / total_accesses;
    float miss_rate = (float) miss / total_accesses;
    float comp_rate = (miss > 0) ? (float) miss_comp / miss : 0;
    float conf_rate = (miss > 0) ? (float) miss_conf / miss : 0;
    float cap_rate = (miss > 0) ? (float) miss_cap / miss : 0;

    if (flagOut == 0) {
        printf("Total de acessos: %d\n", total_accesses);
        printf("Taxa de hit: %.6f%%\n", hit_rate * 100);
        printf("Taxa de miss: %.6f%%\n", miss_rate * 100);
        printf("Misses compulsórios: %.6f%%\n", comp_rate * 100);
        printf("Misses de conflito: %.6f%%\n", conf_rate * 100);
        printf("Misses de capacidade: %.6f%%\n", cap_rate * 100);
    } else {
        printf("%d, %.6f, %.6f, %.6f, %.6f, %.6f\n", total_accesses, hit_rate, miss_rate, comp_rate, conf_rate, cap_rate);
    }
}

// Função principal
int main(int argc, char *argv[]) {
    if (argc != 7) {
        printf("Numero de argumentos incorreto. Utilize:\n");
        printf("./cache_simulator <nsets> <bsize> <assoc> <substituição> <flag_saida> arquivo_de_entrada\n");
        exit(EXIT_FAILURE);
    }

    int nsets = atoi(argv[1]);
    int bsize = atoi(argv[2]);
    int assoc = atoi(argv[3]);
    char subst = argv[4][0];
    int flagOut = atoi(argv[5]);
    char *arquivoEntrada = argv[6];

    srand(time(NULL));

    printf("nsets = %d\n", nsets);
    printf("bsize = %d\n", bsize);
    printf("assoc = %d\n", assoc);
    printf("subst = %c\n", subst);
    printf("flagOut = %d\n", flagOut);
    printf("arquivo = %s\n", arquivoEntrada);

    Cache *cache = init_cache(nsets, bsize, assoc, subst);
    read_address_file(arquivoEntrada, cache);

    for (int i = 0; i < nsets; i++) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);

    return 0;
}
