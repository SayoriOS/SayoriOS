; Hello World - FASM
format ELF
public main

; Входная точка
main:
	; Выводим сообщение из message
	mov eax, 0   		; 0 - сисфункция вывода принта
	mov ebx, message 	; параметры сисфункции, в нашем случае строка для вывода
	int 80h 			; Вызов прерывания

	; В eax получаем случайное число
	mov eax, 20   	; 20 - сисфункция rand
	int 80h 		; Вызов прерывания
	mov ebx, eax 	; Перемещает вывод функции rand в первый аргумент сисфункции ebx(для принта)

	; Выводим содержимое ebx полученное из сисфункции rand
	mov eax, 0 	; 0- сисфункция принта
	int 80h 	; Вызывает прерывание

	ret			; Выход

; Сообщение сохраняем в message, также попрошу заметить:
; 10 это \n
; 0 это конец строки
message db '#ILOVESYNAPSEOS', 10, 'xd)', 10, 0
