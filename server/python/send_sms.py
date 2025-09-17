import os
from twilio.rest import Client
from twilio.base.exceptions import TwilioRestException


def send_sms_func(body, from_, to):
    # Find your Account SID and Auth Token at twilio.com/console
    # and set the environment variables. See http://twil.io/secure
    client = Client("{redacted}", "{redacted}")

    try:
        message = client.messages.create(body=body, from_=from_, to=to)
        # print("Successfully sent message: " + str(message))
        return True, str(message)
    except TwilioRestException as e:
        # print("Caught exception when sending SMS: " + str(e))
        return False, str(e)


print("Initializing send_sms.py file")
