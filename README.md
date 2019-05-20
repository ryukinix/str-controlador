# str-controlador

Aquecedor e Controlador de Software em Tempo Real usando TCP/IP Socket
UDP. Um diagrama a seguir pode ser visto de como funciona a aplicação.

![imagem](imagens/esquema_projeto.png)

O uso de threads é gerenciado pela biblioteca `pthread` de C, onde
cada uma dessas threads são funções que executam tarefas periódicas:
uma thread para ler os sensores e salvar num vetor; outra thread
apenas responsabilizada para imprimir o que foi lido.

# Autores

+ Pedro Renoir Silveira Sampaio - 389113
+ Samuel Hericles Souza Silveira - 389118
+ Geronimo Pereira Aguiar - 385145
+ Manoel Vilela Machado Neto - 394192

# Como executar

Rode o servidor com este comando:

``` shell
make server
```

Clique em simular.

Rode então o cliente com este comando:

``` shell
make run
```


Tente atualizar os sensores pela interface do servidor e verifique a
saída do cliente.


# Dependências:

+ make
+ gcc
+ java

Em um sistema
``` shell
sudo apt install build-essentials default-jre -y
```
