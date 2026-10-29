FROM python:3.13-slim
WORKDIR /app
COPY server.py /app/server.py
ENV PYTHONUNBUFFERED=1
EXPOSE 8080
CMD ["python","server.py"]
