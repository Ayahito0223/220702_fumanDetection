import socket
import os
import ipaddress
import hashlib
import threading

from dotenv import load_dotenv
from slack import SlackAPI

M_SIZE = 1024
arduino_socket = None
slack = None

def arduinoResponce():
  global M_SIZE, arduino_socket, slack
  counter = 1
  while True:
    message, addr = arduino_socket.recvfrom(M_SIZE)
    if message.decode(encoding='utf-8') == '2':
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
  global M_SIZE, arduino_socket, slack

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
    if message.decode(encoding='utf-8') == '1':
      print("接続に成功しました。")
      break
    else:
      print("接続に失敗しました。")
      print('')

  slack = SlackAPI(SLACK_BOT_USER_OAUTH_TOKEN)

  threading.Thread(target=arduinoResponce, daemon=True).start()

  print("プログラムを終わる場合は、q")
  while True:
    mes = input()
    if mes == 'q':
      arduino_socket.sendto('3'.encode('utf-8'), (arduino_ip_address, arduino_port))
      exit()

if __name__ == "__main__":
  main()