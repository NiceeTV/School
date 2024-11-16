import socket
import threading
import time
import tkinter as tk
from tkinter import filedialog
import os

class Peer:
    def __init__(self,ip,port,server_ip,server_port,fragment_size) -> None:
        self.sender_thread = None
        self.listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.listen_sock.bind((ip,port))


        self.server_ip = server_ip
        self.server_port = server_port
        self.max_fragment_size = fragment_size

        self.sqn = 0
        self.ack = 0
        self.expected_sqn = 0
        self.last_sent_paket = None #posledne poslaný paket, pre prípad straty
        self.print_lock = threading.Lock()

        self.connected = False
        self.accepted = False
        self.other_closed = False #druhý peer ukončil svoje posielanie
        self.me_closed = False #ja som prerušil posielanie
        self.ready = True
        self.exit = False

        #musí si otvoriť porty na počúvanie
        self.listener_thread = threading.Thread(target=self.receive)
        self.listener_thread.start()

        #vlákno na vstup
        self.sender_thread = threading.Thread(target=self.interface)
        self.sender_thread.start()

        #vlákna na otvorenie spojenia
        self.establish_thread = threading.Thread(target=self.establish_connection)
        self.establish_thread.start()



    def establish_connection(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        print("Establishing connection ...")

        count = 0
        while not self.accepted: #posielaj každých 6s SYN paket
            packet_id = 1
            self.sqn = 0
            self.ack = 0

            syn_packet = self.create_packet("", packet_id, 0x00, self.sqn,self.ack, 1)
            sock.sendto(syn_packet, (self.server_ip, self.server_port))

            with self.print_lock:
                print(f"Odoslaný SYN: SQN={self.sqn}, ACK={self.ack}")
            time.sleep(6)
            count += 1

            if count == 1: #36s a ukončí spojenie
                print("Spojenie sa nepodarilo, druhá strana neodpovedá.")
                self.exit = True
                break

        sock.close()



    def interface(self):
        while not self.exit:
            if self.connected:
                if self.ready:
                    choice = input("Chcete odoslať (1) správu alebo (2) súbor (nepodporované) alebo (3) nesprávny paket? (zadajte 1 alebo 2, alebo 'q' na ukončenie): ")
                    if choice == '1':
                        self.change_fragment_size()

                        message = input("Zadajte správu: ") #výzva na zadanie správy
                        self.send_message(message)
                    elif choice == '2':
                        self.change_fragment_size()

                        #výzva na výber súboru
                        print("Tento program zatiaľ nepodporuje túto službu.")
                        #file_path = input("Zadajte cestu k súboru: ")
                        #self.send_file(file_path)

                    elif choice == '3': #poslanie zlého paketu
                        self.change_fragment_size()

                        message = input("Zadajte správu: ") #výzva na zadanie správy
                        self.send_false_packet(message)

                    elif choice.lower() == 'q':
                        #termination TO-DO
                        self.terminate_connection()
                        break
                    else:
                        print("Neplatná voľba, skúste znova.")



    def change_fragment_size(self):
        valid = False
        choice = None

        while not valid:
            with self.print_lock:
                choice = input("Chcete zmenit maximálnu veľkosť fragmentov? y/n ")

            if choice == 'y' or choice == 'Y' or choice == 'n' or choice == 'N':
                valid = True


        if choice == 'y' or choice == 'Y':  # výzva na zadanie veľkosti
            fragment_size = 0
            while True:
                print("Zadajte maximálnu veľkosť fragmentu: ")
                try:
                    fragment_size = int(input())
                except ValueError:
                    continue

                if 0 < max_fragment_size <= 1462: #max limit fragmentu pre moju hlavičku
                    break

            self.max_fragment_size = fragment_size


    def send_message(self,message):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender_socket:

            self.sqn += len(message)
            self.last_sent_paket = self.create_packet(bytes(message,encoding='utf-8'),1,0x04,self.sqn,self.ack,1)
            self.expected_sqn = self.sqn

            sender_socket.sendto(self.last_sent_paket, (target_ip, sending_port))
            with self.print_lock:
                self.ready = False
                print(f"Odoslaná správa: {message}")


    def send_false_packet(self,message):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender_socket:

            self.sqn += len(message)
            self.last_sent_paket = self.create_packet(bytes(message, encoding='utf-8'), 1, 0x04, self.sqn, self.ack, 1)
            self.expected_sqn = self.sqn
            false_packet = self.last_sent_paket.copy() #kopia paketu

            #vytvoríme chybu, pridáme zlé crc
            false_packet[9] = 0xFF
            false_packet[10] = 0xFF

            sender_socket.sendto(false_packet, (target_ip, sending_port))
            with self.print_lock:
                self.ready = False
                print(f"Odoslaná správa: {message}")


    def send_file(self,file_path): #to-do
        pass


    def receive(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        while not self.exit:
            response, address = self.listen_sock.recvfrom(1024)
            sqn = (response[2] << 8) | response[3]
            ack = (response[4] << 8) | response[5]

            if not self.connected:
                if response[1] == 0x00: #ak dostane SYN, prestane posielať svoj SYN a zahaji handshake
                    self.accepted = True

                    #idem poslať SYN-ACK
                    self.ack = sqn + 1
                    syn_ack_packet = self.create_packet(bytes("", encoding='utf-8'), 1, 0x01, self.sqn, self.ack, 1)
                    sock.sendto(syn_ack_packet, (self.server_ip, self.server_port))
                    with self.print_lock:
                        print(f"Prijatý SYN: SQN={sqn}, ACK={ack}")
                        print(f"Odoslaný SYN-ACK: SQN={self.sqn}, ACK={self.ack}")


                if response[1] == 0x01: #SYN-ACK, pošli zase ACK
                    self.sqn = ack
                    self.ack = sqn + 1

                    ack_packet = self.create_packet(bytes("", encoding='utf-8'), 1, 0x02, self.sqn, self.ack, 1)
                    sock.sendto(ack_packet, (self.server_ip, self.server_port))
                    with self.print_lock:
                        print(f"Prijatý SYN-ACK: SQN={sqn}, ACK={ack}")
                        print(f"Odoslaný ACK: SQN={self.sqn}, ACK={self.ack}")

                    #zahájena komunikácia
                    self.accepted = True
                    self.connected = True
                    print("Zacala komunikacia")

                if response[1] == 0x02:
                    self.connected = True

                    print(f"Prijatý ACK: SQN={sqn}, ACK={ack}")
                    print("Zacala komunikacia")


            if self.connected: #uz komunikacia s datami/subormi
                self.ack = (response[2] << 8) | response[3]
                self.sqn = (response[4] << 8) | response[5]

                if response[1] == 0x06: #FIN paket, pošlem späť ACK paketň
                    ack_packet = self.create_packet(bytes("", encoding='utf-8'), 1, 0x02, self.sqn, self.ack, 1)
                    sock.sendto(ack_packet, (self.server_ip, self.server_port))
                    #peer2 sa rozhodol, že už nebude posielať pakety, ale my ešte môžeme posielať jemu

                    if self.me_closed:
                        #koniec
                        self.listen_sock.close()
                        print("Koniec komunikácie.")
                        self.ready = True
                        break
                    else: #ešte nekončí komunikácia, čaká sa na moj FIN
                        with self.print_lock:
                            print(f"\nPeer ({target_ip}) už nebude posielať žiadne pakety.")
                            print("Chcete odoslať (1) správu alebo (2) súbor (nepodporované) alebo (3) nesprávny paket? (zadajte 1 alebo 2, alebo 'q' na ukončenie): ")

                        self.other_closed = True


                if response[1] == 0x04: #to je správa
                    sprava = response[11:].decode()
                    with self.print_lock:
                        print(f"\nPrijatá správa: {sprava} (SQN={self.sqn}, ACK={self.ack})")
                        self.ready = True


                    #kontrola checksum
                    crc = (response[9] << 8) | response[10]

                    new_crc = self.crc16(response[11:])
                    if new_crc == crc:
                        with self.print_lock:
                            print("Paket je korektný.")
                            if not self.me_closed:
                                print("Chcete odoslať (1) správu alebo (2) súbor (nepodporované) alebo (3) nesprávny paket? (zadajte 1 alebo 2, alebo 'q' na ukončenie): ")

                        #pošleme ACK paket ako potvrdenie
                        ack_packet = self.create_packet(bytes("", encoding='utf-8'), 1, 0x02, self.sqn, self.ack, 1)
                        sock.sendto(ack_packet, (self.server_ip, self.server_port))

                    else:
                        with self.print_lock:
                            print("Paket je poškodený.")

                        if self.last_sent_paket is not None:
                            last_packet_length = (self.last_sent_paket[7] << 8) | self.last_sent_paket[8]
                            self.ack -= last_packet_length #musíme sa vrátiť na posledný správne odoslaný paket

                        #pošleme NACK paket, vyžiadame si jeho opatovne poslanie
                        nack_packet = self.create_packet(bytes("", encoding='utf-8'), 1, 0x03, self.sqn, self.ack, 1)
                        sock.sendto(nack_packet, (self.server_ip, self.server_port))

                if response[1] == 0x03:  # NACK, pošli ešte raz paket
                    sock.sendto(self.last_sent_paket, (target_ip, sending_port))
                    with self.print_lock:
                        print("Poškodený paket. Posielam znova.")

                if response[1] == 0x02: #ACK, že prišla správa

                    if ack == self.expected_sqn: #prišlo všetko v poriadku
                        with self.print_lock:
                            print("Správa prišla prijímateľovi správne.")
                            self.ready = True

                    if self.other_closed:
                        #koniec spojenia
                        self.listen_sock.close()
                        print("Koniec komunikácie.")
                        break
        self.listen_sock.close()


    def terminate_connection(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender_socket:
            self.sqn += 1
            self.expected_sqn = self.sqn

            fin_packet = self.create_packet(bytes("", encoding='utf-8'), 1, 0x06, self.sqn, self.ack, 1)
            sender_socket.sendto(fin_packet, (self.server_ip, self.server_port))
            self.me_closed = True
            print("Ukončili ste posielanie paketov.")


    def create_packet(self,data: bytes,packet_id,typ,sqn,ack,total):
        length = len(data)

        crc = self.crc16(data)
        header = bytearray()
        header.append(packet_id)
        header.append(typ)

        #sqn
        header.append((sqn >> 8) & 0xFF)  #prvý bajt
        header.append(sqn & 0xFF)   #druhý bajt

        #ack
        header.append((ack >> 8) & 0xFF)
        header.append(ack & 0xFF)

        header.append(total)

        # length v bajtoch je na 2 bajty uložená
        header.append((length >> 8) & 0xFF)
        header.append(length & 0xFF)


        packet = header

        #crc16
        packet.append((crc >> 8) & 0xFF)
        packet.append(crc & 0xFF)

        packet.extend(data)

        return packet

    def crc16(self,data: bytes) -> int:  #CCITT-FALSE
        INITIAL_VALUE = 0xFFFF
        POLYNOMIAL = 0x1021

        crc = INITIAL_VALUE  #inicializácia CRC na počiatočnú hodnotu
        for byte in data:
            crc ^= (byte << 8)  #XOR s aktuálnym bajtom (posun doľava)
            for _ in range(8):  #pre každý bit v bajte
                if (crc & 0x8000):  #ak je najvyšší bit 1
                    crc = (crc << 1) ^ POLYNOMIAL  #posun doľava a XOR s polynomom
                else:
                    crc <<= 1  #len posun doľava
                crc &= 0xFFFF  #obmedzenie na 16 bitov
        return crc ^ 0x0000


#nastavenie parametrov
listening_port = 0
sending_port = 0
target_ip = 0
max_fragment_size = 0


while True:
    print("Zadajte port na počúvanie: ")
    try:
        listening_port = int(input())
    except ValueError:
        continue
    break

while True:
    print("Zadajte cieľovú IP adresu: ")

    target_ip = input()
    l = target_ip.split(".")

    valid = True
    if len(l) == 4: #musia byť 4 čísla
        for c in l:
            if int(c) < 0 or int(c) > 255: #neplatný rozsah
                valid = False
                break

        if valid: #platná ip adresa
           break

while True:
    print("Zadajte port na posielanie: ")
    try:
        sending_port = int(input())
    except ValueError:
        continue
    break

while True:
    print("Zadajte maximálnu veľkosť fragmentu: ")
    try:
        max_fragment_size = int(input())
    except ValueError:
        continue

    if max_fragment_size > 0:
        break

#TOTO TREBA ZMENIT PRED ODOVZDANIM
CLIENT_IP = "192.168.1.169" #naša IP


peer = Peer(CLIENT_IP,listening_port,target_ip,sending_port,max_fragment_size)

