FROM node:22-bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends g++-mingw-w64-x86-64 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY package.json ./
RUN npm install --only=production

COPY . .

EXPOSE 3000

CMD ["node", "server.js"]
