from slack_sdk import WebClient

class SlackAPI():
  token = ''
  client = None

  def __init__(self, token):
    self.token = token
    self.client = WebClient(token=token)

  def sendMessage(self, text):
    return self.client.chat_postMessage(channel="#general", text=text)