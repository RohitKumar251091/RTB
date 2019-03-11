import smtplib

threshold = 30


def mail_dispatch():
    # creates SMTP session
    s = smtplib.SMTP('smtp.gmail.com', 587)
    s.starttls()
    s.login("4rohit25@gmail.com", "XXXXXXXXX")
    subject = "Temprature exceed"
    body = "Hello this is for test"
    message = 'Subject: {}\n\n{}'.format(subject, body)
    s.sendmail("4rohit25@gmail.com", "rohitcheeku25@gmail.com", message)
    s.quit()


def validator(data):
    if float(data) > float(threshold):
        mail_dispatch()
