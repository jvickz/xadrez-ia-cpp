# Testes no Windows Sandbox

Esta alternativa executa os mesmos binários em um ambiente descartável. Não altera o Smart App Control nem resolve a assinatura no Windows principal.

Pré-requisito: ativar **Windows Sandbox / Área Restrita do Windows** em **Ativar ou desativar recursos do Windows**, com uma conta de administrador, e reiniciar se solicitado.

Compile o jogo e os testes com `./build.ps1 -BuildTests` (adicione `-Compiler` se necessário). Depois, na pasta do projeto, execute `./sandbox/preparar-testes.ps1`. O script copia os executáveis para `%LOCALAPPDATA%\XadrezIA-Sandbox`, confere seus hashes e abre `Testar-xadrez.wsb`. Depois da preparação, também é possível abrir o `.wsb` diretamente. Rode a preparação novamente após recompilar o jogo.

O mapeamento direto da unidade virtual G: do Google Drive retornou `0x80070005` (acesso negado). A preparação usa o disco local para o compartilhamento, sem mudar os arquivos originais nem as políticas de segurança.

A configuração desativa rede, microfone, câmera, impressoras e compartilhamento da área de transferência. As cópias locais em `build` e `runner` são compartilhadas somente para leitura; apenas a pasta local `results` recebe arquivos. Os executáveis são copiados para a pasta temporária dentro do ambiente descartável antes dos testes.

O procedimento executa testes de regras/IA, interface/eventos de áudio, persistência em processos separados, salvamento automático após encerramento abrupto e reprodução dos seis efeitos. Cada execução grava relatórios com identificador próprio em `%LOCALAPPDATA%\XadrezIA-Sandbox\results` e abre o jogo para conferência manual. O runner retorna código 1 se alguma etapa falhar. Os relatórios podem ser copiados de volta para `sandbox-results` no projeto; essa pasta fica fora do versionamento.

O resumo de cada execução identifica o resultado de todas as etapas; código 0 significa sucesso. A revisão para divulgação está registrada em [docs/REVISAO.md](../docs/REVISAO.md). Os logs locais não são pré-requisito para compilar ou testar em outro computador.

Uma execução bem-sucedida dentro do Sandbox não garante a reprodução no dispositivo de áudio do Windows principal. O teste de áudio verifica a aceitação pela API, não se o usuário ouviu o som.

Referências oficiais: [instalação](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-install) e [configuração](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file).
