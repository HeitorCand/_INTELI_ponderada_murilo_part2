FROM golang:1.22-alpine

WORKDIR /app

COPY consumer/ .

RUN go mod init consumer
RUN go get github.com/streadway/amqp
RUN go get github.com/lib/pq
RUN go build -o consumer

CMD ["./consumer"]