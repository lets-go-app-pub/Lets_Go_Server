import socket
import urllib.error

import sendgrid
from sendgrid.helpers.mail import *


def send_email_func(email_prefix, to_email_str, email_subject_str, email_content_str):
    email_string = email_prefix + "@letsgoapp.site"
    sg = sendgrid.SendGridAPIClient(api_key="{redacted}")
    from_email_ = Email(email_string)
    to_email_ = To(to_email_str)
    subject_ = email_subject_str
    # NOTE: can also use text/plain however text/html allows for html properties to be used in the content string
    content_ = Content("text/html", email_content_str)
    mail_ = Mail(from_email_, to_email_, subject_, content_)

    try:
        response = sg.send(mail_)
        # response.status_code is the Hypertext Transfer Protocol (HTTP) Status Code
        status_code_return_value = response.status_code
        header_return_value = response.headers
        body_return_value = response.headers
    except socket.gaierror:
        # print("socket.gaierror")
        status_code_return_value = -1
        header_return_value = "socket.gaierror"
        body_return_value = "socket.gaierror"
    except urllib.error.URLError:
        # print("urllib.error.URLError")
        status_code_return_value = -1
        header_return_value = "urllib.error.URLError"
        body_return_value = "urllib.error.URLError"

    # print(header_return_value)
    # print(body_return_value)

    return status_code_return_value, str(header_return_value), str(body_return_value)


print("Initializing send_email.py file")


def testing_send_email():
    email_prefix = "LetsGoEmailVerification"
    generated_link = "http://127.0.0.1:8000/"  #
    to_email_str = "testemail123@gmail.com"
    email_subject_str = "LetsGo! Email Verification"
    email_content_str = "Click the link below to verify your email address.\n\n\n<a href='" + \
        generated_link + "'><h3>Verification Link</h3></a> "

    print("testing_send_email running!\n")
    email_string = email_prefix + "@letsgoapp.site"
    sg = sendgrid.SendGridAPIClient(api_key="{redacted}")
    from_email_ = Email(email_string)
    to_email_ = To(to_email_str)
    subject_ = email_subject_str
    # NOTE: can also use text/plain however text/html allows for html properties to be used in the content string
    content_ = Content("text/html", email_content_str)
    mail_ = Mail(from_email_, to_email_, subject_, content_)

    try:
        response = sg.send(mail_)
        status_code_return_value = response.status_code
        header_return_value = response.headers
        body_return_value = response.headers
    except socket.gaierror:
        print("socket.gaierror")
        status_code_return_value = -1
        header_return_value = "socket.gaierror"
        body_return_value = "socket.gaierror"
    except urllib.error.URLError:
        print("urllib.error.URLError")
        status_code_return_value = -1
        header_return_value = "urllib.error.URLError"
        body_return_value = "urllib.error.URLError"

    print(header_return_value)
    print(body_return_value)

    return status_code_return_value, str(header_return_value), str(body_return_value)
