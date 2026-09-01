# ETAPAS — trilha de aprendizado

Este arquivo divide o projeto em etapas pequenas, cada uma compilável e testável,
e explica a API do Linux à medida que você implementa. A ideia é **você escrever o código** —
os trechos aqui servem de apoio e referência.

## Como usar este guia

1. Faça as etapas **em ordem** — cada uma é um incremento pequeno e testável.
2. Compile e teste **antes** de avançar (use `curl`; instruções no fim).
3. Não copie o código inteiro: escreva você mesmo e use os trechos como apoio/confronto.
4. Marque o progresso nos checkbox `[ ]` de cada etapa.

## Etapa 0 — Preparar o ambiente

```bash
sudo apt install gcc make qrencode      # Debian/Ubuntu
sudo pacman -S gcc make qrencode        # Arch
```

**qrencode** é opcional (usado na Etapa 8). Para testar HTTP localmente, `curl` já resolve.

## Etapa 1 — Servidor TCP mínimo

**Objetivo:** um programa que escuta na porta 8080 e responde algo no navegador.

**O que você vai aprender:** o núcleo de networking do Linux. Todo servidor TCP segue o mesmo ciclo de 4 chamadas:

```
socket()  ->  bind()  ->  listen()  ->  accept() -> (loop: atender cliente)
```

| Chamada | O que faz |
|---------|-----------|
| `socket(AF_INET, SOCK_STREAM, 0)` | Cria um descritor de arquivo (fd) que representa o socket. `AF_INET` = IPv4, `SOCK_STREAM` = TCP (orientado a conexão, como HTTP). |
| `bind(fd, (addr)sockaddr_in, tam)` | Liga o socket a um endereço + porta concretos. `struct sockaddr_in` guarda IP (`sin_addr`) e porta (`sin_port`, em *network byte order* — use `htons()`). |
| `listen(fd, backlog)` | Diz ao kernel: "aceite conexões entrantes; enfileire até `backlog` delas". |
| `accept(fd, NULL, NULL)` | **Bloqueia** até alguém conectar e devolve um *novo* fd de conexão — o fd original continua escutando. |

**Snippet de apoio (`src/server.c`):**

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};           // zeroa a struct
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(8080);      // porta em network byte order
    addr.sin_addr.s_addr = INADDR_ANY;       // aceita conexões de qualquer IP

    bind(fd, (struct sockaddr *)&addr, sizeof addr);
    listen(fd, 16);

    for (;;) {
        int c = accept(fd, NULL, NULL);
        const char *res = "HTTP/1.1 200 OK\r\n"
                          "Content-Length: 2\r\n"
                          "Connection: close\r\n"
                          "\r\nOK";
        write(c, res, strlen(res));
        close(c);
    }
}
```

Compile: `gcc -Wall -o server src/server.c`

**Teste:** em outro terminal, `curl http://localhost:8080` → deve imprimir `OK`. Abra `http://localhost:8080` no navegador também.

### Perguntas para se fazer
- Por que `htons(8080)`? (byte order: kernel espera big-endian na rede)
- O que acontece se 2 processos tentarem usar a porta 8080 ao mesmo tempo?
- Por que `accept()` retorna um fd **novo** em vez de reusar o original?
- Dica: se o `bind` falhar com "Address already in use", estude `SO_REUSEADDR` (opção `setsockopt`).

## Etapa 2 — Servir um arquivo de verdade

**Objetivo:** responder com o conteúdo de um arquivo, ex. `foto.jpg`.

**O que você vai aprender:**
- `open("/caminho/arquivo", O_RDONLY)` → fd do arquivo
- `fstat(fd, &st)` → metadados, incluindo `st.st_size` (tamanho em bytes)
- Loop de leitura em **blocos**: `read(fd, buf, BUFSIZE)` + `send(c, buf, n, 0)` até `read` retornar 0 (EOF). Usar um buffer de 4 KB faz o programa aguentar arquivos gigantes sem carregar tudo na RAM.

```c
#define BUFSIZE 4096

char buf[BUFSIZE];
ssize_t n;
while ((n = read(fdfile, buf, BUFSIZE)) > 0)
    send(fdconn, buf, n, 0);          // n pode ser menor que BUFSIZE no fim
```

**Headers HTTP mínimos da resposta:**
```
HTTP/1.1 200 OK
Content-Length: <tamanho do arquivo>      <- obrigatório para o navegador saber quando acabou
Content-Type: image/jpeg
Connection: close
```

> **Por que `Content-Length` importa?** Sem ele, o navegador fica esperando os dados "eternos" até fechar a conexão — funciona, mas é feio. Com ele, o download avança e termina corretamente.

**Teste:** rode o servidor e `curl -o /tmp/baixado.jpg http://localhost:8080`; compare com o original usando `cmp foto.jpg /tmp/baixado.jpg`.

### Perguntas para se fazer
- O que `read` retorna quando o arquivo acaba? E se der erro em disco?
- Por que enviar `n` em vez de `BUFSIZE` no `send`?
- Por que o servidor precisa `open()`ar o arquivo e não apenas ler bytes soltos?

## Etapa 3 — Disparar download no navegador (`Content-Disposition`)

**Objetivo:** fazer o navegador do celular baixar em vez de exibir.

**O que você vai aprender:** o parâmetro `attachment` da header `Content-Disposition` — ela não muda o corpo, só instrui o navegador a tratar como download com o nome indicado:

```
Content-Disposition: attachment; filename="foto.jpg"
```

**Teste:** abra no navegador com e sem essa header e veja a diferença no comportamento.

## Etapa 4 — Parser mínimo de GET

**Objetivo:** não responder para qualquer coisa, tratar apenas `GET /` e ignorar o resto (favicon, robots, etc.).

**O que você vai aprender:** a primeira linha de uma requisição HTTP segue o formato `MÉTODO ESPAÇO CAMINHO ESPAÇO HTTP/x.y`. Você precisa de um buffer, um `recv()` inicial e pouca string-machinery (`strncmp`, pegar até o primeiro espaço).

- Se for `GET` → serve o arquivo.
- Qualquer outra coisa → `404`, `405`, ou simplesmente ignora e responde `404`.

**Teste:** `curl -X POST http://localhost:8080` deve receber 404/405; `curl http://localhost:8080/favicon.ico` também.

## Etapa 5 — Descobrir o IP local (`getifaddrs`)

**Objetivo:** imprimir a URL correta (`http://<ip>:8080`) em vez de forçar o usuário a descobrir o IP na mão.

**O que você vai aprender:** `getifaddrs(struct ifaddrs **list)` retorna uma lista ligada com todas as interfaces de rede (lo, wlan0, eth0...). Cada nó tem `ifa_name`, `ifa_addr` (pode ser NULL!) e flags. Filtrar:
- `ifa_addr != NULL`
- família `AF_INET` (IPv4)
- **não** ser loopback (`lo`)
- preferir a que tem IP válido em rede privada (começa com `10.`, `192.168.`, `172.16-31.`)

```c
#include <ifaddrs.h>

struct ifaddrs *list = NULL;
getifaddrs(&list);                 // aloca a lista internamente
// ...percorrer, achar o IP, copiar para um buffer...
freeifaddrs(list);                 // SEMPRE liberar
```

`struct sockaddr_in *ipv4 = (struct sockaddr_in *)ifa->ifa_addr;` depois `inet_ntop(AF_INET, &ipv4->sin_addr, str, 46)` converte o endereço binário para texto.

**Teste:** rode `ip addr` (ou `ifconfig`) e confira que o IP impresso pelo programa bate com o da interface ativa.

## Etapa 6 — Sair limpo com Ctrl+C (SIGINT)

**Objetivo:** encerrar o loop principal no Ctrl+C, imprimir uma mensagem e fechar o socket sem "processo morto pelo sinal".

**O que você vai aprender:** sinais são formas assíncronas do kernel avisar o processo. `SIGINT` é o gerado pelo Ctrl+C. Duas formas de tratar:

1. `signal(SIGINT, handler)` — simples (versão antiga/padrão).
2. `sigaction(SIGINT, &sa, NULL)` — moderna/recomendada: controla flags como `SA_RESTART`.

Um **handler deve ser minúsculo**: a prática comum é apenas setar uma variável global `volatile sig_atomic_t g_running = 0;` (com tipo que seja atômico/seguro) e deixar o loop principal checar:

```c
volatile sig_atomic_t g_running = 1;

void on_sigint(int sig) { (void)sig; g_running = 0; }

// no main: loop usa while (g_running) { ... }
```

> O handler NÃO pode chamar `printf` com segurança — só setar a flag é seguro.

**Teste:** inicie o servidor, faça um download (ou deixe o navegador aberto) e aperte Ctrl+C. O processo deve sair logo com mensagem limpa, sem "Interrupted".

## Etapa 7 — Shell: `scripts/instalar.sh`

**Objetivo:** um script que compila o C e instala o binário num lugar do `PATH`.

**O que você vai aprender em shell:**
- `#!/bin/bash` (shebang) + `chmod +x instalar.sh`
- `set -e` → aborta se qualquer comando falhar
- variáveis: `BIN="$HOME/bin"`, `TARGET="sendfile"`
- `gcc -Wall -O2 -o "$TARGET" ../src/server.c`
- `install -m 755 "$TARGET" "$BIN/$TARGET"` (ou `mkdir -p "$BIN"` antes) — note a **substituição** até poderia ser `cp`, mas `install` já lida com permissões
- `echo "[OK] instalado em $BIN/$TARGET"` e `exit 0` (sucesso) / `exit 1` (falha)

```bash
#!/bin/bash
set -e
BIN="${HOME}/bin"
mkdir -p "$BIN"
gcc -Wall -O2 -o sendfile ../src/server.c
install -m 755 sendfile "$BIN/sendfile"
echo "[OK] Instalado em $BIN/sendfile"
```

**Teste:** rode `bash scripts/instalar.sh` e depois `command -v sendfile`.

## Etapa 8 — Shell: `scripts/sendfile.sh` com QR code

**Objetivo:** chamar o servidor e, enquanto ele roda, exibir um QR code no terminal apontando para `http://<ip>:8080` — o celular lê e abre na hora.

**O que você vai aprender em shell:**
- como o script obtém a URL: idealmente o binário C **imprime a URL** (Etapa 5); o script captura com `$(...)` (substituição de comando)
- pipes: `echo "$URL" | qrencode -t ANSIUTF8` — `qrencode` lê o texto de entrada padrão e desenha no terminal (ASCII/UTF-8), deixando o URL QR-code navegável
- rodar o servidor em **background** (`&`), esperar 1s, mostrar o QR, depois `wait` no processo
- `trap` para garantir que o background morra junto com o script (Ctrl+C)

```bash
#!/bin/bash
FILE="${1:?Uso: sendfile.sh <arquivo>}"
URL="$(sendfile "$FILE")"           # binário imprime ex: http://192.168.0.15:8080
echo "Acesse no celular: $URL"
echo "$URL" | qrencode -t ANSIUTF8
```

Você terá a liberdade de decidir **onde** o servidor roda: ele pode continuar front (bloqueando o script) — então o `qrencode` precisa rodar antes — ou em background (`$!` para guardar o PID e `wait $!`).

**Teste:** aponte a câmera do celular para o QR (mesma Wi-Fi!) e confirme o download.

---

## Dicas de teste (importantes!)

1. **Mesma máquina:** `curl -v http://localhost:8080` — o `-v` mostra headers e o corpo.
2. **Saiba o IP:** `ip -brief addr` antes de testar no celular.
3. **Celular:** confira que ele está no **mesmo Wi-Fi** (não em dados móveis) e que o firewall do PC não bloqueia: `sudo ufw allow 8080/tcp` se necessário.
4. **Conferir integridade:** `cmp` entre o arquivo original e o baixado.
5. **Medir velocidade:** `curl -o /dev/null http://192.168.x.x:8080` mostra quanto tempo levou.
6. Se travar, `Ctrl+Z` pausa o processo e `jobs`/`fg` o retoma — útil ao depurar.
