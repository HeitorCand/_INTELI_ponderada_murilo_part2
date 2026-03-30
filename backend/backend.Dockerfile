FROM golang:1.22-alpine

WORKDIR /app

COPY backend/ .

RUN go mod download
RUN go build -o app

CMD ["./app"]