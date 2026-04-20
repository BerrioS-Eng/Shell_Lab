# >_ Shell_Lab
Implementing a _command line interpreter_ (CLI).

## 🚀 Members
- Sebastian Andres Berrio Murillo, CC. 1066753315, sebastian.berriom@udea.edu.co.
- Sandy Dahiana Ruiz Higuita, CC. 1028031733, sandy.ruizh@udea.edu.co - sadrh97@gmail.com.

##Parallel Commands
El shell desarrollado permite la ejecución paralela que se implementa usando el operador &. La lógica general consiste en dividir la línea de entrada en múltiples comandos independientes y ejecutarlos de forma concurrente.
Primero se realiza una validación de sintaxis para evitar casos inválidos cómo && o el uso de & al inicio o al final de la línea. Si se detecta alguno de estos casos, se imprime el mensaje de error y se continúa con la siguiente iteración del shell.

Luego la línea que escribió el usuario se divide en varias partes usando el símbolo & como separador. Cada parte entonces corresponde a los comandos a ejecutar. Pero puede pasar el caso donde el usuario escriba algo como "ls & & pwd", para este caso, cuando se separa la línea queda un espacio vacío entre los dos &, lo que es un comando que no existe y entonces, cuando el programa detecta este caso lo toma como error y no ejecuta ningún comando en esa línea y muestra el mensaje de error.

Cuando el programa obtiene los comandos válidos, el shell recorre cada uno y crea un proceso hijo mediante el fork(). Cada hijo ejecuta su comando correspondiente usando execv() dentro de la función execute_command. El  proceso padre no espera inmediatamente, sino que almacena los pid de cada hijo en un arreglo.

Después de lanzar todos los procesos en paralelo, el padre utiliza waitpid(), en un ciclo para esperar a que todos los procesos hijos terminen. Con esto garantizamos que el shell no muestre el prompt nuevamente hasta que todos los comandos hayan finalizado.

##Program Errors
El manejo de errores, se basa en la cadena definda como:
char error_message[30] = "An error has ocurred\n"
Tal como se definió en las indicaciones, cada vez que ocurre un error, este mensaje se imprime usando write() hacia STDERR_FILENO, cumpliendo con la especificación.

Los distintos errores se manejan en diferentes puntos del programa, tenemos
1. Errores de invocación del programa:
   - Si se pasan más de un argumento al ejecutar el shell, se imprime el error y se termina con exit(1).
   - Si el archivo en modo batch no se puede abrir, también se imprime el error y se termina con exit(1).
2. Errores de sintaxis:
   - Uso incorrecto de & (como && o posiciones invalida, mencionado anterormente en el manejo de paralelismo)
   - Comandos vacíos generados por separaciones incorrectas
   - Uso incorrecto del operador > (como más de una redirección o formato inválido)
3. Errores en ejecución
   - Comando no encontrado
   - Fallo en execv()
   - Fallo al abrir archivos para redirección
En la mayoría de los casos, el shell imprime el mensaje de error y continúa ejecutandose. Solo en errores críticos, como los de inicialización, el programa termina, todo esto siguiendo las especificaciones dadas.

## Problemas encontrados y soluciones
Durante el proceso tuvimos varios problemas
- Doble fork innecesario: Inicialmente estabamos haciendo un fork() tanto en el main como en execute_command, lo que generaba procesos duplicados. Cuando detectamos dicho error y después de considerar la lógica a usar en el programa, lo solucionamos dejando un solo fork que debía ir únicamente en el main.
- Manejo incorrecto de comandos vacíos con &: Al principio, casos como cmd1 && cmd2 no eran detectados y después de distintas pruebas y de darnos cuenta de esto, lo corregimos validando explícitamente comandos vacíos después de usar strsep()

## Pruebas realizadas
Se realizaron pruebas para verificar tanto la ejecución en paralelo como el manejo de errores:

#Pruebas de paralelismo
- sleep 2 & sleep 3 & ls: Permite verificar que los comandos se ejecutan simultáneamente
- pwd & ls & echo hola: Confirma ejecución concurrente de múltiples comandos.
#Pruebas de errores de sintaxis
- ls && pwd
- & ls
- ls &
- ls & & pwd
