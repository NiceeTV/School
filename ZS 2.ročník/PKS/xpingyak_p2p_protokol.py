import logging
import os
import socket
import threading
import time
import tkinter as tk
from tkinter.filedialog import askopenfilename
import asyncio

MAX_FRAGMENT_SIZE = 1459 #daná protokolom

class Peer:
    def __init__(self,ip,port,server_ip,server_port,fragment_size) -> None:
        self.received_packets = []
        self.sender_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.listen_sock.settimeout(36) #po 36s prestane prijímať pakety a považuje pokus o komunikáciu neúspešný
        self.listen_sock.bind((ip,port))

        self.server_ip = server_ip
        self.server_port = server_port
        self.max_fragment_size = fragment_size

        self.sqn = 0
        self.ack = 0
        self.expected_sqn = 0
        self.last_sent_paket = None #posledne poslaný paket, pre prípad straty
        self.print_lock = threading.Lock()

        self.connected = False #sme pripojený
        self.accepted = False #prijatie handshaku druhou stranou
        self.other_closed = False #druhý peer ukončil svoje posielanie
        self.me_closed = False #ja som prerušil posielanie
        self.ready = True #je možný výpis
        self.exit = False #ukončenie programu
        self.transfer = False #presun pri fragmentácii


        #prenos súborov
        self.received_fragments = {}  # Ukladá prijaté fragmenty podľa SQN
        self.total_fragments = 0  # Celkový počet fragmentov
        self.file_name = None  # Meno prijímaného súboru
        self.file_size = 0

        self.n_to_corrupt = 0

        logging.basicConfig(
            filename="logfile.log",  # Názov log súboru
            level=logging.DEBUG,  # Úroveň logovania (DEBUG, INFO, WARNING, ERROR, CRITICAL)
            format='%(asctime)s - %(levelname)s - %(message)s',  # Formát log správy
        )


        #musí si otvoriť porty na počúvanie
        self.listener_thread = threading.Thread(target=self.receive)
        self.listener_thread.start()

        #vlákno na vstup
        self.sender_thread = threading.Thread(target=self.interface)
        self.sender_thread.start()

        #vlákna na otvorenie spojenia
        self.establish_thread = threading.Thread(target=self.establish_connection)
        self.establish_thread.start()

        self.listener_thread.join()  # Čakáme, kým sa listener vlákno dokončí
        self.sender_thread.join()  # Čakáme, kým sa sender vlákno dokončí
        self.establish_thread.join()




    def log_message(self,level, message):
        if level == 'INFO':
            logging.info(message)
        elif level == 'ERROR':
            logging.error(message)
        elif level == 'WARNING':
            logging.warning(message)
        elif level == 'DEBUG':
            logging.debug(message)
        else:
            logging.critical(message)

    def establish_connection(self):
        self.log_message('INFO',"Establishing connection ...")
        print("Establishing connection ...")

        count = 0
        while not self.accepted: #posielaj každých 6s SYN paket
            self.sqn = 0
            self.ack = 0

            self.expected_sqn = self.sqn + 1
            syn_packet = self.create_packet(b"",0x00,self.sqn,self.ack)
            self.sender_socket.sendto(syn_packet, (self.server_ip, self.server_port))
            self.last_sent_paket = syn_packet

            self.log_message('DEBUG',f"Odoslaný SYN: SQN={self.sqn}, ACK={self.ack}")
            time.sleep(6)
            count += 1

            if count == 6: #36s a ukončí posielanie sám
                self.log_message('ERROR',"Spojenie sa nepodarilo, druhá strana neodpovedá.")
                with self.print_lock:
                    print("Spojenie sa nepodarilo, druhá strana neodpovedá.")
                self.exit = True
                self.sender_socket.close()
                break

    def interface(self):
        while not self.exit:
            if self.connected:
                if self.ready:
                    with self.print_lock:
                        choice = input("Chcete odoslať (1) správu alebo (2) súbor (nepodporované) alebo (3) nesprávny paket? (zadajte 1 alebo 2, alebo 'q' na ukončenie): ")
                    if choice == '1':
                        self.change_fragment_size()
                        with self.print_lock:
                            message = input("Zadajte správu: ") #výzva na zadanie správy
                        if len(message) > self.max_fragment_size:
                            #treba ju fragmentovat
                            pass
                        else:
                            self.send_message(message) #pošleme ako celok


                    elif choice == '2':
                        self.change_fragment_size()

                        #výzva na výber súboru
                        root = tk.Tk()
                        root.withdraw()
                        root.attributes("-topmost", True)


                        file_path = askopenfilename(title="Vyberte súbor")
                        file_size = os.path.getsize(file_path)


                        if file_size > self.max_fragment_size:
                            #treba fragmentovať
                            fragmented = self.fragmentation(file_path,self.max_fragment_size)

                            corrupt = input("Chcete simulovať poškodenie fragmentov? y/n")
                            if corrupt == 'y' or corrupt == 'Y':
                                n = int(input("Zvoľte n pre simulovanie poškodenia každého n-tého fragmentu:"))
                                while n > len(fragmented)-1:
                                    n = int(input("Zvoľte n pre simulovanie poškodenia každého n-tého fragmentu:"))

                                self.n_to_corrupt = n

                            self.send_fragmented(file_path,file_size,fragmented)

                        else:
                            #môže sa poslať celé
                            pass


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

    def retransmit(self,packet_id):
        """Retransmit a specific packet"""
        if not self.acks_received[packet_id]:
            print(f"Retransmitting fragment {packet_id}")
            packet = self.fragmented[packet_id]
            fragment_sqn = packet_id
            self.log_message('DEBUG', f"Retransmitting fragment {packet_id}, sqn {fragment_sqn} ack {self.ack}")
            f_packet = self.create_packet(packet, 0x05, fragment_sqn, self.ack, length=len(packet))
            self.sender_socket.sendto(f_packet, (self.server_ip, self.server_port))
            self.last_sent_paket = f_packet

            # Reset timer for this packet
            if packet_id in self.timers:
                self.timers[packet_id].cancel()
            self.timers[packet_id] = threading.Timer(5.0, self.retransmit, args=[packet_id])
            self.timers[packet_id].start()


    def send_fragmented(self,file_path,file_size,fragmented):
        #v prvom rade inicializácia
        self.file_name = os.path.basename(file_path)
        self.file_size = file_size
        self.total_fragments = len(fragmented)
        self.fragmented = fragmented

        #pošli inicializacny paket
        self.expected_sqn = self.sqn + 1
        print(f"pred posielanim SQN {self.sqn} ACK {self.ack} expected_sqn {self.expected_sqn}")

        init_paket = self.create_packet(b"",0x07,self.sqn,self.ack,name_length=len(self.file_name),name=self.file_name,file_size=self.file_size,fragment_size=self.max_fragment_size)
        self.sender_socket.sendto(init_paket, (self.server_ip, self.server_port))
        self.last_sent_paket = init_paket


        self.transfer = False

        wait_start = time.time()
        timeout = 10  # 10 sekúnd timeout

        while not self.transfer:
            if time.time() - wait_start > timeout:
                print("Timeout pri čakaní na potvrdenie inicializácie prenosu")
                self.log_message('ERROR', "Timeout pri čakaní na potvrdenie inicializácie prenosu")
                return
            time.sleep(0.1)


        self.log_message('INFO',"Začína prenos.")
        start = time.time()

        window_size = 10

        next_seq_num = 0

        #treba zverejniť inej funkcii
        self.base = 0
        self.acks_received = [False] * self.total_fragments  # Sledovanie potvrdení
        self.timers = {}  # Časovače pre jednotlivé pakety

        self.log_message('DEBUG',f"Posielam súbor {self.file_name} o veľkosti {self.file_size} bajtov v {self.total_fragments} fragmentoch.")
        self.log_message('DEBUG',f"Inicializačný paket má sqn {self.sqn} a ack {self.ack}.")
        print(f"Posielam súbor {self.file_name} o veľkosti {self.file_size} bajtov v {self.total_fragments} fragmentoch.")

        # Main transfer loop
        try:
            while self.base < self.total_fragments:
                # Send new packets within window
                while next_seq_num < self.base + window_size and next_seq_num < self.total_fragments:
                    packet = fragmented[next_seq_num]
                    # Calculate correct sequence number for each fragment
                    fragment_sqn = next_seq_num
                    f_packet = self.create_packet(packet, 0x05, fragment_sqn, self.ack, length=len(packet))
                    if fragment_sqn % self.n_to_corrupt == 0: #každý n-tý fragment na simulovanie korupcie, vytvoríme chybu, pridáme zlé crc
                        f_copy = f_packet.copy()
                        f_copy[11] = 0xFF
                        f_copy[12] = 0xFF
                        self.sender_socket.sendto(f_copy, (self.server_ip, self.server_port))
                        print(f"Posielam poškodený paket {fragment_sqn}")
                    else:
                        self.sender_socket.sendto(f_packet, (self.server_ip, self.server_port))

                    self.last_sent_paket = f_packet
                    self.log_message('DEBUG',f"Sent fragment {next_seq_num} with SQN {fragment_sqn} ACK {self.ack}")

                    print(f"Sent fragment {next_seq_num} with SQN {fragment_sqn}")

                    # Start timer for this packet
                    if next_seq_num in self.timers and self.timers[next_seq_num].is_alive():
                        self.timers[next_seq_num].cancel()
                    self.timers[next_seq_num] = threading.Timer(5.0, self.retransmit, args=[next_seq_num])
                    self.timers[next_seq_num].start()

                    next_seq_num += 1



        except Exception as e:
            self.log_message('ERROR',f"Error during transfer: {e}")
            with self.print_lock:
                print(f"Error during transfer: {e}")
            # Clean up timers on error
            for timer in self.timers.values():
                if timer.is_alive():
                    timer.cancel()
            raise

        # Clean up timers after successful transfer
        for timer in self.timers.values():
            if timer.is_alive():
                timer.cancel()

        self.transfer = False
        self.log_message('INFO',f"Posielanie súboru {self.file_name} s veľkosťou {self.file_size} a veľkosťou fragmentu {self.max_fragment_size} dokončené.")


        print("Posielanie súboru dokončené.")

        end = time.time()
        print("Time elapsed: ",end-start)


    def handle_fragment(self, packet):
        """Spracovanie prijatého fragmentu."""
        sqn = (packet[1] << 24) | (packet[2] << 16) | (packet[3] << 8) | packet[4]
        length = (packet[9] << 8) | packet[10]
        data = packet[13:13 + length]
        crc_received = (packet[11] << 8) | packet[12]

        # Overenie CRC
        if self.crc16(data) != crc_received:
            self.log_message('ERROR',f"Fragment {sqn} je poškodený. Vyžiadanie opätovného odoslania. Checksum: {self.crc16(data)} Prijatý Checksum: {crc_received}")
            nack_packet = self.create_packet(b"", 0x03, sqn, self.ack)
            self.sender_socket.sendto(nack_packet, (self.server_ip, self.server_port))
            return

        # Ak fragment nie je poškodený, ulož ho
        self.received_fragments[sqn] = data
        self.log_message('DEBUG',f"Fragment {sqn} prijatý a uložený. Checksum: {crc_received}")

        print(f"Fragment {sqn} prijatý a uložený. Checksum: {crc_received}")

        # Poslanie ACK pre prijatý fragment
        ack_packet = self.create_packet(b"", 0x02, sqn, self.ack)
        self.sender_socket.sendto(ack_packet, (self.server_ip, self.server_port))

        # Skontroluj, či máme všetky fragmenty
        if len(self.received_fragments) == self.total_fragments:
            self.log_message("DEBUG","Všetky fragmenty prijaté. Skladanie súboru.")
            self.ack += self.file_size
            self.reassemble_file()

    def reassemble_file(self):
        """Poskladá prijaté fragmenty do súboru."""

        sorted_fragments = [self.received_fragments[i] for i in sorted(self.received_fragments.keys())]
        with open(self.file_name, 'wb') as file:
            for fragment in sorted_fragments:
                file.write(fragment)
        self.log_message("INFO",f"Súbor {self.file_name} s veľkosťou {self.file_size} bol úspešne poskladaný a uložený.")
        self.transfer = False

        print(f"Súbor {self.file_name} úspešne poskladaný a uložený.")
        print(f"SQN {self.sqn} ACK {self.ack}")
        print("Chcete odoslať (1) správu alebo (2) súbor (nepodporované) alebo (3) nesprávny paket? (zadajte 1 alebo 2, alebo 'q' na ukončenie): ")

    def fragmentation(self,file_path,fragment_size=MAX_FRAGMENT_SIZE):
            fragments = []
            with open(file_path, 'rb') as file:  # Otvorenie súboru v binárnom režime
                while True:
                    chunk = file.read(fragment_size)  # Načítanie fragmentu
                    if not chunk:
                        break  # Ak už nie sú dáta, ukončíme čítanie
                    fragments.append(chunk)
            return fragments

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
                with self.print_lock:
                    print("Zadajte maximálnu veľkosť fragmentu: ")
                try:
                    fragment_size = int(input())
                except ValueError:
                    continue

                if 0 < max_fragment_size <= MAX_FRAGMENT_SIZE: #max limit fragmentu pre moju hlavičku
                    break

            self.max_fragment_size = fragment_size
            self.log_message('INFO',f"Bola zmenená veľkosť fragmentov na {self.max_fragment_size} Bajtov.")

    def send_message(self,message):
        #self.sqn += len(message)
        self.last_sent_paket = self.create_packet(bytes(message,encoding='utf-8'),0x04,self.sqn,self.ack,length=len(message))
        self.expected_sqn = self.sqn + len(message)

        self.sender_socket.sendto(self.last_sent_paket, (target_ip, sending_port))
        self.log_message('INFO', f"Odoslaná správa: {message}")
        with self.print_lock:
            self.ready = False
            print(f"Odoslaná správa: {message}")

    def send_false_packet(self,message):
        #self.sqn += len(message)
        self.last_sent_paket = self.create_packet(bytes(message, encoding='utf-8'),  0x04, self.sqn, self.ack, length=len(message))
        self.expected_sqn = self.sqn + len(message)
        false_packet = self.last_sent_paket.copy() #kopia paketu

        #vytvoríme chybu, pridáme zlé crc
        false_packet[11] = 0xFF
        false_packet[12] = 0xFF

        self.sender_socket.sendto(false_packet, (target_ip, sending_port))
        self.log_message('INFO', f"Odoslaná správa: {message}")
        with self.print_lock:
            self.ready = False
            print(f"Odoslaná správa: {message}")

    def receive(self):
        while not self.exit:
            try:
                response, address = self.listen_sock.recvfrom(MAX_FRAGMENT_SIZE)
                sqn = (response[1] << 24) | (response[2] << 16) | (response[3] << 8) | response[4]
                ack = (response[5] << 24) | (response[6] << 16) | (response[7] << 8) | response[8]

            except socket.timeout:
                self.log_message('ERROR',"Čas na prijatie komunikačného paketu vypršal. Ukončujem program.")
                print("Čas na prijatie komunikačného paketu vypršal. Ukončujem program.")
                break

            if not self.connected:
                if response[0] == 0x00: #ak dostane SYN, prestane posielať svoj SYN a zahaji handshake
                    self.accepted = True



                    #idem poslať SYN-ACK


                    self.sqn = ack
                    self.ack = sqn + 1
                    self.expected_sqn = self.sqn + 1


                    #self.expected_sqn += self.sqn + 1
                    syn_ack_packet = self.create_packet(b"",  0x01, self.sqn, self.ack)
                    self.last_sent_paket = syn_ack_packet
                    self.sender_socket.sendto(syn_ack_packet, (self.server_ip, self.server_port))

                    self.log_message('DEBUG',f"Prijatý SYN: SQN={sqn}, ACK={ack}")
                    self.log_message('DEBUG', f"Odoslaný SYN-ACK: SQN={self.sqn}, ACK={self.ack+1}")



                if response[0] == 0x01: #SYN-ACK, pošli zase ACK
                    print("expected synack rec: ",self.expected_sqn,ack)

                    if (self.expected_sqn == ack):

                        self.sqn = ack
                        self.ack = sqn + 1

                        ack_packet = self.create_packet(b"", 0x02, self.sqn, self.ack)
                        self.last_sent_paket = ack_packet

                        self.sender_socket.sendto(ack_packet, (self.server_ip, self.server_port))

                        self.log_message('DEBUG',f"Prijatý SYN-ACK: SQN={sqn}, ACK={ack}")
                        self.log_message('DEBUG',f"Odoslaný ACK: SQN={self.sqn}, ACK={self.ack}")

                        #zahájena komunikácia
                        self.accepted = True
                        self.listen_sock.settimeout(None)
                        self.log_message('INFO', "Zacala komunikacia")

                        print(f"Zacala komunikacia SQN {self.sqn} ACK {self.ack}")
                        self.connected = True

                if response[0] == 0x02:
                    print("expected ack rec: ",self.expected_sqn,ack)
                    if (self.expected_sqn == ack):
                        self.sqn = ack
                        self.ack = sqn

                        self.listen_sock.settimeout(None)
                        self.log_message('INFO', "Zacala komunikacia")
                        self.log_message('DEBUG',f"Prijatý ACK: SQN={sqn}, ACK={ack}")
                        print(f"Zacala komunikacia SQN {self.sqn} ACK {self.ack}")
                        self.connected = True
                        continue


            if self.connected: #uz komunikacia s datami/subormi
                self.ack = (response[1] << 24) | (response[2] << 16) | (response[3] << 8) | response[4]
                self.sqn = (response[5] << 24) | (response[6] << 16) | (response[7] << 8) | response[8]


                if self.transfer:
                    received_sqn = self.sqn
                    fragment_id = received_sqn
                    if response[0] == 0x02 and 0 <= fragment_id < self.total_fragments:  # Valid ACK
                        self.log_message('DEBUG', f"ACK received for fragment {fragment_id} (SQN {received_sqn})")

                        print(f"ACK received for fragment {fragment_id} (SQN {received_sqn})")

                        fragment_payload = len(self.fragmented[fragment_id])
                        self.sqn += fragment_payload
                        self.expected_sqn = self.sqn
                        # Cancel timer and mark as received
                        if fragment_id in self.timers:
                            self.timers[fragment_id].cancel()
                        self.acks_received[fragment_id] = True

                        # Slide window if possible
                        while self.base < self.total_fragments and self.acks_received[self.base]:
                            self.log_message('DEBUG', f"Sliding window, base now {self.base + 1}")
                            self.base += 1


                    if response[0] == 0x03:
                        self.retransmit(fragment_id)
                        self.log_message('DEBUG',f"Resending broken fragment {fragment_id} with SQN {fragment_id} ACK {self.ack}")
                        print(f"Resending broken fragment {fragment_id} with SQN {fragment_id}")
                    continue






                if response[0] == 0x07: #inicializacný paket, pošli späť ACK

                    ack_packet = self.create_packet(b"", 0x02, self.sqn, self.ack+1)
                    print("jeho sqn ",self.ack)
                    self.last_sent_paket = ack_packet
                    self.sender_socket.sendto(ack_packet, (self.server_ip, self.server_port))
                    self.log_message('DEBUG',f"Odoslaná odpoveď na inicializáciu sqn {self.sqn} ack {self.ack+1}")
                    self.log_message('INFO',f"Prišla výzva na prenos dát. Začína prenos sqn {self.ack} ack {self.sqn}")
                    #self.transfer = True

                    #rozobrať si ten paket a získať dôležité informácie
                    name_length = (response[9] << 8) | response[10]
                    self.file_name = response[11:11+name_length]
                    poz = 11+name_length
                    self.file_size = int.from_bytes(response[poz+1:poz+4], byteorder='big')
                    self.max_fragment_size = (response[poz+4] << 8) | response[poz+5]
                    self.total_fragments = (self.file_size + self.max_fragment_size - 1) // self.max_fragment_size
                    self.received_fragments = {}

                    self.log_message('INFO',f"Prijímam súbor {self.file_name} o veľkosti {self.file_size} bajtov, počte fragmentov {self.total_fragments} a veľkosti fragmentov {self.max_fragment_size}.")
                    self.log_message('INFO', f"Veľkosť posledného fragmentu bude {self.file_size % self.max_fragment_size} Bajtov.")

                    print(f"Prijímam súbor {self.file_name} o veľkosti {self.file_size} bajtov, počte fragmentov {self.total_fragments} a veľkosti fragmentov {self.max_fragment_size}.")
                    print(f"Veľkosť posledného fragmentu bude {self.file_size % self.max_fragment_size} Bajtov.")



                if response[0] == 0x05: #fragment
                    self.handle_fragment(response)



                if response[0] == 0x06: #FIN paket, pošlem späť ACK paket
                    ack_packet = self.create_packet(b"", 0x02, self.sqn, self.ack+1)

                    self.sender_socket.sendto(ack_packet, (self.server_ip, self.server_port))
                    #peer2 sa rozhodol, že už nebude posielať pakety, ale my ešte môžeme posielať jemu

                    if self.me_closed:
                        #koniec
                        self.listen_sock.close()
                        self.log_message('INFO',"Koniec komunikácie.")

                        print("Koniec komunikácie.")
                        self.ready = True
                        break
                    else: #ešte nekončí komunikácia, čaká sa na moj FIN
                        self.log_message('INFO',f"\nPeer ({target_ip}) už nebude posielať žiadne pakety.")

                        print(f"\nPeer ({target_ip}) už nebude posielať žiadne pakety.")
                        print("Chcete odoslať (1) správu alebo (2) súbor (nepodporované) alebo (3) nesprávny paket? (zadajte 1 alebo 2, alebo 'q' na ukončenie): ")

                        self.other_closed = True


                if response[0] == 0x04: #to je správa, nefragmentovaná
                    length = (response[9] << 8) | response[10]
                    sprava = response[13:13+length]
                    self.log_message('INFO',f"\nPrijatá správa: {sprava.decode()} (SQN={self.sqn}, ACK={self.ack})")

                    print(f"\nPrijatá správa: {sprava.decode()} (SQN={self.sqn}, ACK={self.ack})")
                    self.ready = True


                    #kontrola checksum
                    crc = (response[11] << 8) | response[12]

                    new_crc = self.crc16(sprava)
                    if new_crc == crc:
                        self.log_message('DEBUG',"Paket je korektný.")

                        if not self.me_closed:
                            print("Chcete odoslať (1) správu alebo (2) súbor (nepodporované) alebo (3) nesprávny paket? (zadajte 1 alebo 2, alebo 'q' na ukončenie): ")

                        #pošleme ACK paket ako potvrdenie
                        ack_packet = self.create_packet(b"", 0x02, self.sqn, self.ack+len(sprava))
                        self.sender_socket.sendto(ack_packet, (self.server_ip, self.server_port))

                    else:
                        self.log_message('ERROR',"Paket je poškodený.")

                        print("Paket je poškodený.")

                        if self.last_sent_paket is not None:
                            last_packet_length = (self.last_sent_paket[9] << 8) | self.last_sent_paket[10]
                            self.ack -= last_packet_length #musíme sa vrátiť na posledný správne odoslaný paket

                        #pošleme NACK paket, vyžiadame si jeho opatovne poslanie
                        nack_packet = self.create_packet(bytes("", encoding='utf-8'), 0x03, self.sqn, self.ack)
                        self.sender_socket.sendto(nack_packet, (self.server_ip, self.server_port))

                if response[0] == 0x03:  # NACK, pošli ešte raz paket
                    self.sender_socket.sendto(self.last_sent_paket, (target_ip, sending_port))
                    self.log_message('ERROR',"Poškodený paket. Posielam znova.")

                    print("Poškodený paket. Posielam znova.")

                if response[0] == 0x02: #ACK, že prišla správa
                    print("expected ",self.expected_sqn,ack)
                    if ack == self.expected_sqn: #prišlo všetko v poriadku
                        if self.last_sent_paket is not None:
                            if self.last_sent_paket[0] == 0x07:
                                self.sqn = ack
                                self.transfer = True
                                self.log_message('INFO',"Prijímateľ prijal našu výzvu na prenos dát.")

                        self.log_message('INFO',"Správa prišla prijímateľovi správne.")
                        self.ready = True

                    if self.other_closed:
                        #koniec spojenia
                        self.listen_sock.close()
                        self.log_message('INFO',"Koniec komunikácie.")

                        print("Koniec komunikácie.")
                        break

        self.listen_sock.close()


    def terminate_connection(self):
        #self.sqn += 1
        self.expected_sqn = self.sqn

        fin_packet = self.create_packet(bytes("", encoding='utf-8'), 0x06, self.sqn, self.ack)
        self.sender_socket.sendto(fin_packet, (self.server_ip, self.server_port))
        self.me_closed = True
        self.log_message('INFO',"Ukončili ste posielanie paketov.")
        with self.print_lock:
            print("Ukončili ste posielanie paketov.")

    def create_packet(self,data: bytes,typ,sqn,ack,length=0,name_length=0,name="",file_size=0,fragment_size=MAX_FRAGMENT_SIZE):
        length = len(data)

        crc = self.crc16(data)
        header = bytearray()

        header.append(typ) #typ paketu

        #sqn
        header.append((sqn >> 24) & 0xFF)  # Najvyšší bajt
        header.append((sqn >> 16) & 0xFF)
        header.append((sqn >> 8) & 0xFF)  #prvý bajt
        header.append(sqn & 0xFF)   #druhý bajt

        #ack
        header.append((ack >> 24) & 0xFF)  # Najvyšší bajt
        header.append((ack >> 16) & 0xFF)
        header.append((ack >> 8) & 0xFF)
        header.append(ack & 0xFF)

        #tu sa rozdeluje hlavička dynamicky

        if length != 0: #ide o normálny paket
            # length v bajtoch je na 2 bajty uložená
            header.append((length >> 8) & 0xFF)
            header.append(length & 0xFF)


        #tu to môže byť réžijný alebo inicializačný (prenos súboru) paket
        if name_length != 0: #inicializacny paket
            #name length, dĺžka názvu súboru
            header.append((name_length >> 8) & 0xFF)
            header.append(name_length & 0xFF)

            #názov súboru
            header.extend(bytes(name,encoding='utf-8'))

            #file size
            header.append((file_size >> 24) & 0xFF)  # Najvyšší bajt
            header.append((file_size >> 16) & 0xFF)
            header.append((file_size >> 8) & 0xFF)
            header.append(file_size & 0xFF)

            #fragment size
            header.append((fragment_size >> 8) & 0xFF)
            header.append(fragment_size & 0xFF)


        #ak nemáme name_length, tak ide o rezijnu správu
        packet = header

        # crc16
        packet.append((crc >> 8) & 0xFF)
        packet.append(crc & 0xFF)
        #dáta/payload
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

    if 0 < max_fragment_size <= MAX_FRAGMENT_SIZE:
        break

#TOTO TREBA ZMENIT PRED ODOVZDANIM
CLIENT_IP = "192.168.0.105" #naša IP


peer = Peer(CLIENT_IP,listening_port,target_ip,sending_port,max_fragment_size)



#pridáme možnosť corruptnut každý n-ty paket aby sme to akože nasimulovali, ale musí to najprv fungovať normálne
#todo spraviť ešte posielanie fragmentovanej správy a nefragmentovaneho súboru
#todo nack posielanie podla id
#todo negunfujú sqn a ack pri prenose init paketu a iné