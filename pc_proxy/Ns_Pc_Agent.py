#!/usr/bin/env python3
# proxy_to_deepseek.py
# Run on PC: python proxy_to_deepseek.py
# Switch connects to http://<PC_IP>:8888

import json
import httpx
from fastapi import FastAPI, Request
from fastapi.responses import StreamingResponse

app = FastAPI()
DEEPSEEK_URL = "https://api.deepseek.com/v1/chat/completions"

@app.post("/v1/chat/completions")
async def proxy(request: Request):
    body = await request.json()
    headers = dict(request.headers)

    async def stream():
        async with httpx.AsyncClient(timeout=120) as client:
            async with client.stream(
                "POST", DEEPSEEK_URL,
                json=body,
                headers={
                    "Authorization": headers.get("authorization", ""),
                    "Content-Type": "application/json",
                }
            ) as resp:
                async for chunk in resp.aiter_bytes():
                    yield chunk

    if body.get("stream"):
        return StreamingResponse(stream(), media_type="text/event-stream")
    else:
        async with httpx.AsyncClient() as client:
            resp = await client.post(DEEPSEEK_URL, json=body, headers=headers)
            return resp.json()

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8888)