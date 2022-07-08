import socket
import os
import ipaddress
import hashlib
import threading
import random
import string
import time

from dotenv import load_dotenv
from slack import SlackAPI

USER_INVALID = '0'
USER_VALID = '1'
HUMAN_DETECTED = '2'
SERVER_OK = '5'
SERVER_QUIT = '9'

M_SIZE = 1024
arduino_socket = None
arduino_ip_address = None
arduino_port = None
streamPass = ''
streamPassCounter = 0
STREAM_PASS_SIZE = 32

slack = None

def checkHashMessage(original):
  global streamPass, streamPassCounter
  result = hashlib.sha256((original + streamPass[streamPassCounter]).encode()).hexdigest()
  streamPassCounter = (streamPassCounter + 1) % STREAM_PASS_SIZE
  return result

def useHashMassage(message):
  global streamPass, streamPassCounter
  result = hashlib.sha256((message + streamPass[streamPassCounter]).encode()).hexdigest().encode('utf-8')
  streamPassCounter = (streamPassCounter + 1) % STREAM_PASS_SIZE
  return result

def arduinoResponce():
  global M_SIZE, arduino_socket, slack, arduino_ip_address, arduino_port
  counter = 1
  while True:
    message, addr = arduino_socket.recvfrom(M_SIZE)
    if message.decode(encoding='utf-8') == checkHashMessage(HUMAN_DETECTED):
      arduino_socket.sendto(useHashMassage(SERVER_OK), (arduino_ip_address, arduino_port))
      if counter == 1:
        result = slack.sendMessage("human detected!!!!!!!")
        if result.get('ok'):
          print('無事送信されました。')
        else:
          print('エラーが起きたようです。')
          print(result)
      elif counter == 10:
        counter = 0
      counter += 1

def main():
  global M_SIZE, arduino_socket, slack, streamPass, arduino_ip_address, arduino_port

  print(hashlib.sha256('abc'.encode()).hexdigest())
  load_dotenv()
  SLACK_BOT_USER_OAUTH_TOKEN = os.getenv('SLACK_BOT_USER_OAUTH_TOKEN')

  while True:
    try:
      arduino_ip_address_str = input("arduinoのIPアドレスを入力してください> ")
      arduino_ip_address = str(ipaddress.ip_address(arduino_ip_address_str))
      break
    except ValueError:
      print('ipアドレスではありません。')
      print('')
  
  while True:
    try:
      arduino_port = int(input("arduinoのポートを入力してください> "))
      break
    except ValueError:
      print("数字で入力してください。")
      print('')

  arduino_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

  while True:
    arduino_pass = input("arduinoに接続するためのパスワードを入力してください。> ")
    arduino_socket.sendto(
      str(hashlib.sha256(arduino_pass.encode()).hexdigest()).encode('utf-8'), 
      (arduino_ip_address, arduino_port)
    )
    print('arduinoからのレスポンスを待っています。。。。')
    message, addr = arduino_socket.recvfrom(M_SIZE)
    if message.decode(encoding='utf-8') == USER_VALID:
      randlst = [random.choice(string.ascii_letters + string.digits) for i in range(STREAM_PASS_SIZE)]
      streamPass = ''.join(randlst)
      print(streamPass)
      time.sleep(2)
      arduino_socket.sendto(streamPass.encode('utf-8'), (arduino_ip_address, arduino_port))
      print("接続に成功しました。")
      break
    else:
      print("接続に失敗しました。")
      print('')

  slack = SlackAPI(SLACK_BOT_USER_OAUTH_TOKEN)

  threading.Thread(target=arduinoResponce, daemon=True).start()

  print("プログラムを終わる場合は、q")
  while True:
    try:
      mes = input()
      if mes == 'q':
        arduino_socket.sendto(useHashMassage(SERVER_QUIT), (arduino_ip_address, arduino_port))
        exit()
    except KeyboardInterrupt:
      arduino_socket.sendto(useHashMassage(SERVER_QUIT), (arduino_ip_address, arduino_port))
      exit()

if __name__ == "__main__":
  main()