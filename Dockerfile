FROM alpine:3.15
WORKDIR /opt/wyn-app
COPY ./wyn-app ./
USER app:app
ENTRYPOINT ["./wyn-app"]
