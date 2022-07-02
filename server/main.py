import serial
import os

from dotenv import load_dotenv
from slack import SlackAPI

def main():
  load_dotenv()
  SLACK_BOT_USER_OAUTH_TOKEN = os.getenv('SLACK_BOT_USER_OAUTH_TOKEN')

  slack = SlackAPI(SLACK_BOT_USER_OAUTH_TOKEN)

  ser = serial.Serial('COM5', 9600)
  while True:
    print('データをslackに送信する場合は、y')
    print('プログラムを止める場合は、q')
    userInput = input('> ')
    if userInput == 'y':
      val_arduino = ser.readline()
      val_decoded = repr(val_arduino.decode())[1:-5]
      result = slack.sendMessage(val_decoded)
      if result.get('ok'):
        print('「 {} 」が無事送信されました。'.format(val_decoded))
      else:
        print('エラーが起きたようです。')
        print(result)
    elif userInput == 'q':
      break
    else:
      print('入力内容がおかしいです。')
      print()
  
  ser.close()

if __name__ == "__main__":
  main()