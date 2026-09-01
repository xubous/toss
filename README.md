# toss

Mini servidor HTTP escrito em **C** puro (APIs nativas do Linux) para compartilhar arquivos **sem instalar nada no receptor** — o celular (ou qualquer dispositivo) só abre a URL no navegador ou lê o QR code.

```bash
sendfile foto.jpg
# Servidor HTTP em http://192.168.0.15:8080
# QR code no terminal -> abre no celular, download dispara
```

---

## A ideia

Compartilhar arquivos entre dispositivos **na mesma rede local** usando apenas um navegador, sem apps, sem nuvem, sem contas. Você aponta o servidor para um arquivo, o receptor lê o QR (ou digita a URL) e baixa direto.

```
PC  ──servidor HTTP──▶  http://ip:8080   ◀── navegador ──  celular/tablet/outro PC
   (mesma rede local, sem internet)
```

---

## Estratégia de compartilhamento

O projeto cobre os fluxos principais de transferência:

- **PC → Telefone (download):** o fluxo principal. O PC serve o arquivo via HTTP e o telefone baixa pelo navegador (QR code).
- **Telefone → PC (upload):** caminho inverso, via `POST` (fase 2). O telefone envia para o servidor.
- **PC → PC / Telefone → Telefone:** como o receptor só precisa de um navegador, qualquer dispositivo da rede usa a mesma URL, independente do SO (Windows, macOS, Linux, Android, iOS).

### Compatibilidade

| Lado | Requisito |
|------|-----------|
| **Emissor (PC)** | Linux + `gcc` (compila o C nativo). `qrencode` opcional p/ QR. |
| **Receptor** | Qualquer navegador moderno (Android, iOS, Windows, macOS, Linux). Nenhum app. |
| **Rede** | Mesma rede local / Wi-Fi. Não necessita de internet. |

### O receptor (é só ter navegador)

| SO | Fluxo |
|----|-------|
| Android | Lê o QR com a câmera → abre a URL → download |
| iOS | Mesma coisa, câmera + Safari |
| Windows/macOS/Linux | Digita a URL ou lê o QR |

---

## Planos futuros (para algo mais versátil)

- [ ] **Upload reverso** — telefone → PC via `POST`/multipart.
- [ ] **Compartilhar pastas inteiras** — servir um `index.html` com a lista de arquivos.
- [ ] **Múltiplos clientes simultâneos** — `select()` ou `fork()`.
- [ ] **Barra de progresso no terminal** no lado do emissor.
- [ ] **Fila de arquivos / "caixa de entrada"** — vários uploads acumulados esperando o receptor.
- [ ] **Endpoint REST** para integrar com apps (webhooks, automação).
- [ ] **Suporte a IPv6** além do IPv4.
- [ ] **Transferência direta via WebSocket** para upload sem border multipart.
- [ ] **Túnel para redes externas** (ex.: acesso remoto via NAT) como evolução opcional.

---

## Estrutura do projeto (meta)

```
toss/
├── README.md            # ideia, estratégia e planos (este arquivo)
├── ETAPAS.md            # trilha de aprendizado, etapa por etapa (C e shell)
├── src/
│   └── server.c         # o coração do projeto (C)
└── scripts/
    ├── instalar.sh      # compila com gcc e instala o binário
    ├── sendfile.sh      # chama o binário + QR code (qrencode)
    └── compartilhar_pasta.sh   # (opcional) lista uma pasta inteira
```

> Para a **trilha de aprendizado** (como implementar do zero, com a API do Linux explicada), veja o [**ETAPAS.md**](ETAPAS.md).

---

## Licença

MIT
