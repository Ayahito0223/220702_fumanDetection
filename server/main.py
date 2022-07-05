import serial
import socket
import os
import ipaddress
import hashlib

from dotenv import load_dotenv
from slack import SlackAPI

def main():
  load_dotenv()
  SLACK_BOT_USER_OAUTH_TOKEN = os.getenv('SLACK_BOT_USER_OAUTH_TOKEN')

  M_SIZE = 1024

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
    print('Arduinoからのレスポンスを待っています。。。。')
    message, addr = arduino_socket.recvfrom(M_SIZE)
    print(message.decode(encoding='utf-8'))
    if message.decode(encoding='utf-8') == '1':
      print("接続に成功しました。")
      break
    else:
      print("接続に失敗しました。")
      print('')

  slack = SlackAPI(SLACK_BOT_USER_OAUTH_TOKEN)

  while True:
    message, addr = arduino_socket.recvfrom(M_SIZE)
    if message.decode(encoding='utf-8') == '2':
      print("detected!")

  # while True:
  #   print('データをslackに送信する場合は、y')
  #   print('プログラムを止める場合は、q')
  #   userInput = input('> ')
  #   if userInput == 'y':
  #     val_arduino = ser.readline()
  #     val_decoded = repr(val_arduino.decode())[1:-5]
  #     result = slack.sendMessage(val_decoded)
  #     if result.get('ok'):
  #       print('「 {} 」が無事送信されました。'.format(val_decoded))
  #     else:
  #       print('エラーが起きたようです。')
  #       print(result)
  #   elif userInput == 'q':
  #     break
  #   else:
  #     print('入力内容がおかしいです。')
  #     print()

if __name__ == "__main__":
  main()