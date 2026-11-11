# Revisão para divulgação — 11/11/2026

**Conclusão:** a versão 3.3 tem uma base funcional adequada para demonstração e portfólio. As suítes executadas passaram. Isso não certifica ausência de bugs, força competitiva da IA ou compatibilidade com todos os computadores.

## Escopo e evidências

Foi feita uma revisão direcionada de regras, busca, controlador gráfico, persistência, scripts de compilação/instalação e documentação. O visual foi conferido em uma captura da própria interface, usando uma posição de demonstração com capturas e seis meios-lances. Não foi uma auditoria de segurança completa.

Ambiente: Windows x64, compilador Zig 0.15.2, C++17 com `-O2 -Wall -Wextra -Wpedantic`. Compilação do jogo e das duas suítes concluída sem mensagens de advertência. Testes executados no Windows Sandbox já configurado, sem alterar proteções do sistema.

Última execução: **28098-27480**, relatórios locais em `sandbox-results/28098-27480-*.txt` (ignorados pelo Git).

| Área | Verificação | Resultado |
|---|---|---|
| Regras | Perft inicial até profundidade 4 e Kiwipete até 3; roque, en passant, promoções e empates | Passou |
| IA | Mate em um nos três níveis, mate forçado em três no difícil e limite de resposta | Passou |
| Entrada | Clique; arraste legal/ilegal; soltar fora; Esc; orientação das pretas | Passou |
| Desfazer | Turno completo, capturas, fim de partida e descarte de resposta antiga da IA | Passou |
| Salvamento | Humano/IA, desfazer, último lance, restauração e validação de arquivo | Passou |
| Falha de escrita | Arquivo bloqueado preserva o salvamento anterior e informa falha | Passou |
| Encerramento abrupto | Processo encerra após o lance, sem salvar manualmente; outro processo lê a posição | Passou |
| Som | Seleção/prioridade dos seis efeitos, silenciar e cancelamento de som pendente | Passou |
| Reprodução Windows | API aceitou os seis efeitos PCM | Passou; não mede os alto-falantes |
| Aparência | Tela de demonstração com peças, capturas, histórico, botões e aviso de autosave | Conferida na dimensão capturada |

SHA-256 do executável gráfico recompilado e da versão 3.3 já instalada, conferidos iguais:

```text
482DC7362C3999AB60B773C42BD09A49411EF0579D4BF80B48B0ABB172AFABA8
```

## Ajustes feitos nesta revisão

1. Alinhado o nome padrão gerado por `build.ps1` com o esperado pelo instalador e pelo runner: `xadrez-grafico-v3.exe`.
2. Adicionada a opção `-BuildTests`, que compila ambas as suítes sem tentar executá-las no Windows principal. Com `-Test`, ambas são compiladas antes da execução.
3. Corrigido o retorno do runner: uma etapa com falha agora produz código final 1. Antes, o resumo podia registrar falhas enquanto o script retornava 0. O caminho de sucesso foi executado; o retorno de falha foi conferido no código, sem injeção de falha nesta revisão.
4. Adicionada cobertura automatizada para arraste e orientação das pretas.
5. Organizado o README com imagem, recursos, requisitos, arquitetura, créditos e limites reais.
6. Ampliado o `.gitignore` para evitar versionar executáveis, saves, temporários e resultados locais. A pasta `build` já estava ignorada.
7. Criada `assets/demo.png` a partir da interface real, sem arte gerada para simular funcionalidades.

O código de produção do jogo não precisou de alterações nesta revisão. Não foi necessário substituir a instalação, pois o binário recompilado é idêntico ao instalado.

## Pontos identificados na revisão

- **Repositório:** na revisão inicial, a pasta ainda não era um repositório Git. O material foi preparado para publicação como `jvickz/xadrez-ia-cpp`.
- **Licença:** pendência resolvida na preparação da publicação. O responsável escolheu a MIT, incluída em [LICENSE](../LICENSE).
- **Distribuição:** o executável tem status Authenticode `NotSigned`. A execução na máquina de desenvolvimento e no Sandbox não garante a abertura em PCs de terceiros. Para divulgação inicial, o código e uma demonstração já são úteis; uma versão para download deve comunicar essa limitação.
- **Compatibilidade:** não foram validados outros computadores, Linux/macOS, escalas de DPI diferentes ou tecnologias assistivas.
- **Persistência:** um único slot por usuário; janelas simultâneas escrevem no mesmo arquivo. O som é persistido no próximo salvamento. Não há gestão de múltiplas partidas.
- **Motor:** não possui Elo aferido; não usa histórico de repetição durante a busca. Não anunciar equivalência a Stockfish ou a plataformas comerciais.
- **Regras:** empates seguem as simplificações casuais documentadas; não anunciar arbitragem completa de torneios.
- **Apresentação:** uma imagem real está em [assets/demo.png](../assets/demo.png). O vídeo demonstrativo ainda não foi gravado; o rascunho de divulgação é mantido localmente, fora do repositório.

## Sequência de publicação recomendada na revisão inicial

1. Escolher licença e nome público do repositório.
2. Versionar fontes, scripts, documentação e assets; revisar a lista de arquivos antes do envio.
3. Publicar o repositório com este README e registrar a versão 3.3. Se houver binário, distribuí-lo em uma Release com hash, requisitos e nota sobre assinatura.
4. Gravar uma demonstração curta e publicar no LinkedIn com link para o código.

Referências: [README no GitHub](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-readmes), [licenciamento de repositórios](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository), [Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases).
