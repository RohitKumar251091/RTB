import pymongo
import json
from AlertManager import validator

client = pymongo.MongoClient("mongodb://localhost:27017")
db = client["RTB"]


def sensor_data_handler(msg):
    collection = db["TEMPRATURE"]
    data = str(msg.payload.decode("utf-8", "ignore"))
    file_data = json.loads(data)
    collection.insert_one(file_data)
    client.close()
    validator(float(file_data["TEMPRATURE"]))
