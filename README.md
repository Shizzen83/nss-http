## Compilation

1. Se mettre à la racine du projet

2. Build de l'image avec

```bash
UBUNTU_VERSION=20.04 #Adapter si besoin

docker build . -t nss_http:${UBUNTU_VERSION} --build-arg UBUNTU_VERSION=${UBUNTU_VERSION}
```

3. Pour récupérer le script compilé, run un container qui restera en vie avec

```bash
docker run -it --name nss_http --entrypoint bash nss_http:${UBUNTU_VERSION}
```

4. Copier le script avec

```bash
docker cp nss_http:/usr/lib/libnss_http.so.2 /tmp/
```

5. Penser à supprimer le container toujours en vie
```bash
docker rm nss_http
```

6. Penser à installer les deps nécessaires pour faire fonctionner le script
```bash
apt update
apt install \
    --no-install-recommends \
    --assume-yes \
    --quiet \
    ca-certificates \
    curl \
    openssl \
    libcurl4 \
    libjansson4
