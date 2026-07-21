import requests
import json


class RequestAPI():
    def __init__(self, baseUrl):
        self.baseUrl = baseUrl

    def sendFidRequest(self, id, fid):
        url = self.baseUrl + '/upload'
        data = {'fidjson': fid, 'id': id}

        resource = requests.post(url, data)

        if resource.status_code == 200:
            print(resource.json())
            return resource
        else:
            print(resource.text)
            raise Exception(resource)

    def getAlgorithmStatus(self):
        url = self.baseUrl + '/length'
        resource = requests.get(url)

        if resource.status_code == 200:
            res = resource.json()
            print(res)
            return res['data']['length']
        else:
            print(resource.text)
            raise Exception(resource)

    def getAlgorithmData(self):
        url = self.baseUrl + '/result'
        resource = requests.get(url)

        if resource.status_code == 200:
            return resource.json()
        else:
            print(resource.text)
            return False


class HCAPI():
    def __init__(self, baseUrl='http://127.0.0.1:8085'):
        self.baseUrl = baseUrl

    def get_Result(self):
        url = self.baseUrl + '/result'
        resource = requests.get(url)

        if resource.status_code == 200:
            res = resource.json()
            return res["data"]['result']
        else:
            print(resource.text)
            return False

    def get_waiting(self):
        url = self.baseUrl + '/waiting'
        resource = requests.get(url)

        if resource.status_code == 200:
            res = resource.json()
            print("wating", res)
            return res["data"]
        else:
            print(resource.text)
            return False

    def get_start(self):
        url = self.baseUrl + '/start'
        resource = requests.get(url)

        if resource.status_code == 200:
            print(resource.json())
            return resource.json()
        else:
            print(resource.text)
            return False


class BaseRequestAPI():
    def __init__(self, baseUrl='https://mainlab.tiyan.pj.hdzn.cn/sci/tiyan'):
        self.baseUrl = baseUrl

    def saveData(self, param):
        url = self.baseUrl + '/scan-img/imageScanNmr/saveImage'
        param = json.dumps(param, ensure_ascii=False)
        print(param)

        resource = requests.post(url, param, headers={'content-type': 'application/json;charset=UTF-8'})
        print(resource)
        if resource.status_code == 200:
            print(resource.json())
            return resource
        else:
            print(resource.text)
            raise Exception(resource)

    def getData(self):
        url = self.baseUrl + '/scan-img/imageScanNmr/query'
        resource = requests.get(url)
        print(resource)
        if resource.status_code == 200:
            print(resource.json())
            return resource
        else:
            print(resource.text)
            raise Exception(resource)

    def createBatch(self, param):
        url = self.baseUrl + '/scan-img/detectbatch/saveBatch'
        param = json.dumps(param, ensure_ascii=False)

        resource = requests.post(url, param, headers={'content-type': 'application/json;charset=UTF-8'})
        print(resource)
        if resource.status_code == 200:
            print(resource.json())
            return resource.json()
        else:
            print(resource.text)
            raise Exception(resource)