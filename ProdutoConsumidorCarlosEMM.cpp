// Produtor-Consumidor — Comparacao SEQUENCIAL vs PARALELO
// Exemplo didático com impressão alinhada para melhor leitura
// Build: g++ -std=gnu++17 -O2 -Wall -Wextra -pthread produtor_consumidor.cpp -o compare.exe
// Run:   ./compare.exe seq | par | both

#include <deque>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <string>
#include <iomanip>

// Estrutura de configuração: define parâmetros da simulação
struct Config {
    std::string mode = "both";  // modo de execução: "par", "seq" ou "both"
    int cap = 3;                // capacidade máxima do buffer
    int items = 12;             // número de itens a produzir/consumir
    int prod_ms = 120;          // tempo de delay do produtor (ms)
    int cons_ms = 150;          // tempo de delay do consumidor (ms)
};

// -------------------- Funções utilitárias --------------------
using Clock = std::chrono::steady_clock;
// Função auxiliar para medir tempo em ms desde t0
static inline long long elapsed_ms(Clock::time_point t0){
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-t0).count();
}

// Helper para impressão com largura fixa (para logs organizados)
static void log_line(const std::string& who, const std::string& msg, int buf, int cap){
    std::cout << std::left << std::setw(18) << who
              << " | " << std::setw(28) << msg
              << " | buffer=" << std::setw(2) << buf << "/" << cap << "\n";
}

// ======================== Execução PARALELA =========================
// Variáveis globais compartilhadas entre produtor e consumidor
std::deque<int> q;            // buffer FIFO
std::mutex mtx;               // garante exclusão mútua
std::condition_variable cp, cc; // variáveis de condição (produção e consumo)
bool done = false;            // flag indicando fim da produção

// Thread do produtor: insere itens no buffer
static void producer_par(const Config c){
    std::cout << "[INFO] Thread do produtor iniciada.\n";
    for(int i=1; i<=c.items; ++i){
        std::unique_lock<std::mutex> lk(mtx);
        // Aguarda até haver espaço no buffer
        while((int)q.size() >= c.cap) {
            log_line("[Produtor]", "Buffer cheio, aguardando...", (int)q.size(), c.cap);
            cp.wait(lk, [&]{ return (int)q.size() < c.cap; });
        }
        // Produz um novo item
        q.push_back(i);
        log_line("[Produtor]", "Produziu item "+std::to_string(i), (int)q.size(), c.cap);
        cc.notify_one(); // sinaliza consumidor
        lk.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(c.prod_ms));
    }
    // Marca fim da produção
    { std::lock_guard<std::mutex> lk(mtx); done = true; }
    cc.notify_all();
    std::cout << "[INFO] Thread do produtor finalizada.\n";
}

// Thread do consumidor: retira itens do buffer
static void consumer_par(const Config c){
    std::cout << "[INFO] Thread do consumidor iniciada.\n";
    int consumed = 0;
    while(true){
        std::unique_lock<std::mutex> lk(mtx);
        // Se o buffer está vazio, aguarda até haver itens ou o fim da produção
        while(q.empty()){
            if(done) {
                log_line("[Consumidor]", "Fim da producao, total="+std::to_string(consumed), (int)q.size(), c.cap);
                std::cout << "[INFO] Thread do consumidor finalizada.\n";
                return;
            }
            log_line("[Consumidor]", "Buffer vazio, aguardando...", (int)q.size(), c.cap);
            cc.wait(lk, [&]{ return !q.empty() || done; });
        }
        // Consome um item do buffer
        int item = q.front(); q.pop_front(); ++consumed;
        log_line("[Consumidor]", "Consumiu item "+std::to_string(item), (int)q.size(), c.cap);
        cp.notify_one(); // sinaliza produtor
        lk.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(c.cons_ms));
    }
}

// Executa a simulação em modo paralelo (2 threads)
static long long run_parallel(const Config c){
    auto t0 = Clock::now();
    std::thread tp(producer_par, c), tc(consumer_par, c);
    tp.join(); tc.join();
    return elapsed_ms(t0);
}

// ======================== Execução SEQUENCIAL =======================
// Executa simulação sequencial (produtor e consumidor intercalados)
static long long run_sequential(const Config c){
    auto t0 = Clock::now();
    std::deque<int> b; int next=1; int consumed=0;
    while(next <= c.items || !b.empty()){
        // Produção até encher buffer ou acabar itens
        while(next <= c.items && (int)b.size() < c.cap){
            b.push_back(next);
            log_line("[Produtor-SEQ]", "Produziu "+std::to_string(next), (int)b.size(), c.cap);
            ++next;
            std::this_thread::sleep_for(std::chrono::milliseconds(c.prod_ms));
        }
        // Consumo até esvaziar buffer
        while(!b.empty()){
            int item = b.front(); b.pop_front(); ++consumed;
            log_line("[Consumidor-SEQ]", "Consumiu "+std::to_string(item), (int)b.size(), c.cap);
            std::this_thread::sleep_for(std::chrono::milliseconds(c.cons_ms));
        }
    }
    log_line("[Consumidor-SEQ]", "Total consumido="+std::to_string(consumed), 0, c.cap);
    return elapsed_ms(t0);
}

// =========================== Função main ============================
int main(int argc, char** argv){
    Config c;
    // Leitura de parâmetros da linha de comando
    if(argc>=2) c.mode = argv[1];
    if(argc>=3) c.cap = std::stoi(argv[2]);
    if(argc>=4) c.items = std::stoi(argv[3]);
    if(argc>=5) c.prod_ms = std::stoi(argv[4]);
    if(argc>=6) c.cons_ms = std::stoi(argv[5]);

    // Cabeçalho com parâmetros de execução
    std::cout << "\n=== Execucao com parametros ===\n";
    std::cout << "Modo="<< c.mode << " | Capacidade="<< c.cap << " | Itens="<< c.items
              << " | P="<< c.prod_ms << "ms | C="<< c.cons_ms << "ms\n\n";

    if(c.mode == "seq"){
        std::cout << "===== Inicio da execucao SEQUENCIAL =====\n\n";
        auto ms = run_sequential(c);
        std::cout << "\n===== Fim da execucao SEQUENCIAL =====\n";
        std::cout << "[Resumo] SEQUENTIAL total: " << ms << " ms\n";

    } else if(c.mode == "par"){
        std::cout << "===== Inicio da execucao PARALELA =====\n\n";
        auto ms = run_parallel(c);
        std::cout << "\n===== Fim da execucao PARALELA =====\n";
        std::cout << "[Resumo] PARALLEL total: " << ms << " ms\n";

    } else if(c.mode == "both"){
        // Execução sequencial
        std::cout << "===== Inicio da execucao SEQUENCIAL =====\n\n";
        auto ms_seq = run_sequential(c);
        std::cout << "\n===== Fim da execucao SEQUENCIAL =====\n\n";

        // Resetar estado global antes de iniciar threads
        { std::lock_guard<std::mutex> lk(mtx); q.clear(); done=false; }

        // Execução paralela
        std::cout << "===== Inicio da execucao PARALELA =====\n\n";
        auto ms_par = run_parallel(c);
        std::cout << "\n===== Fim da execucao PARALELA =====\n\n";

        // Resumo comparativo final
        std::cout << "[Resumo] SEQUENTIAL total: " << ms_seq << " ms\n";
        std::cout << "[Resumo] PARALLEL   total: " << ms_par << " ms\n";
        std::cout << "[Resumo] Diferenca (seq-par): " << (ms_seq - ms_par) << " ms\n";
    } else {
        std::cerr << "Modo invalido. Use: par | seq | both [cap items prod_ms cons_ms]\n";
        return 1;
    }
    return 0;
}