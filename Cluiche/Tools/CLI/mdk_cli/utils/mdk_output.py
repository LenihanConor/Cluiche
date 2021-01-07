from box import Box
import json

def ok(doc_dict):
    return json.dumps({"body": doc_dict, "status": "ok"})

def error(code, message):
    resp = Box(default_box=True)
    resp.status = "error"
    resp.error.code = code
    resp.error.message = message 
    return resp.to_json()
