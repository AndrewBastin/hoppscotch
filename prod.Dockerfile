FROM node:20-alpine3.19 AS base_builder

WORKDIR /usr/src/app

ENV HOPP_ALLOW_RUNTIME_ENV=true

# Required by @hoppscotch/js-sandbox to build `isolated-vm`
RUN apk add python3 make g++

RUN npm install -g pnpm
COPY pnpm-lock.yaml .
RUN pnpm fetch

COPY . .
RUN pnpm install -f --offline


FROM base_builder AS backend_builder
WORKDIR /usr/src/app/packages/hoppscotch-backend
RUN pnpm exec prisma generate
RUN pnpm run build
RUN pnpm --filter=hoppscotch-backend deploy /dist/backend --prod
WORKDIR /dist/backend
RUN pnpm exec prisma generate

FROM node:20-alpine3.19 AS backend
RUN apk add caddy
RUN npm install -g pnpm

RUN addgroup -S hoppgroup && adduser -S hoppuser -G hoppgroup

COPY --from=base_builder  /usr/src/app/packages/hoppscotch-backend/backend.Caddyfile /etc/caddy/backend.Caddyfile
COPY --from=backend_builder --chown=hoppuser:hoppgroup --chmod=755 /dist/backend /dist/backend
COPY --from=base_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-backend/prod_run.mjs /dist/backend

# Remove the env file to avoid backend copying it in and using it
ENV PRODUCTION="true"
ENV PORT=8080
ENV APP_PORT=${PORT}
ENV DB_URL=${DATABASE_URL}

USER hoppuser

WORKDIR /dist/backend

CMD ["node", "prod_run.mjs"]
EXPOSE 80
EXPOSE 3170

FROM base_builder AS fe_builder
WORKDIR /usr/src/app/packages/hoppscotch-selfhost-web
RUN pnpm run generate

FROM caddy:2-alpine AS app
WORKDIR /site
RUN addgroup -S hoppgroup && adduser -S hoppuser -G hoppgroup

COPY --from=fe_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-selfhost-web/prod_run.mjs /usr
COPY --from=fe_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-selfhost-web/selfhost-web.Caddyfile /etc/caddy/selfhost-web.Caddyfile
COPY --from=fe_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-selfhost-web/dist/ .

RUN apk add nodejs npm

RUN npm install -g @import-meta-env/cli

USER hoppuser

EXPOSE 80
EXPOSE 3000
CMD ["/bin/sh", "-c", "node /usr/prod_run.mjs && caddy run --config /etc/caddy/selfhost-web.Caddyfile --adapter caddyfile"]

FROM base_builder AS sh_admin_builder
WORKDIR /usr/src/app/packages/hoppscotch-sh-admin
# Generate two builds for `sh-admin`, one based on subpath-access and the regular build
RUN pnpm run build --outDir dist-multiport-setup
RUN pnpm run build --outDir dist-subpath-access --base /admin/

FROM caddy:2-alpine AS sh_admin
WORKDIR /site
RUN addgroup -S hoppgroup && adduser -S hoppuser -G hoppgroup

COPY --from=sh_admin_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-sh-admin/prod_run.mjs /usr
COPY --from=sh_admin_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-sh-admin/sh-admin-multiport-setup.Caddyfile /etc/caddy/sh-admin-multiport-setup.Caddyfile
COPY --from=sh_admin_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-sh-admin/sh-admin-subpath-access.Caddyfile /etc/caddy/sh-admin-subpath-access.Caddyfile
COPY --from=sh_admin_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-sh-admin/dist-multiport-setup /site/sh-admin-multiport-setup
COPY --from=sh_admin_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-sh-admin/dist-subpath-access /site/sh-admin-subpath-access

RUN apk add nodejs npm

RUN npm install -g @import-meta-env/cli

USER hoppuser

EXPOSE 80
EXPOSE 3100
CMD ["node","/usr/prod_run.mjs"]

FROM node:20-alpine3.19 AS aio
# Run this separately to use the cache from backend
RUN apk add caddy

RUN apk add tini curl

RUN npm install -g pnpm

RUN addgroup -S hoppgroup && adduser -S hoppuser -G hoppgroup

# Copy necessary files
# Backend files
COPY --from=base_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-backend/backend.Caddyfile /etc/caddy/backend.Caddyfile
COPY --from=backend_builder --chown=hoppuser:hoppgroup --chmod=755 /dist/backend /dist/backend
COPY --from=base_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-backend/prod_run.mjs /dist/backend

# FE Files
COPY --from=base_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/aio_run.mjs /usr/src/app/aio_run.mjs
COPY --from=fe_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-selfhost-web/dist /site/selfhost-web
COPY --from=sh_admin_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-sh-admin/dist-multiport-setup /site/sh-admin-multiport-setup
COPY --from=sh_admin_builder --chown=hoppuser:hoppgroup --chmod=755 /usr/src/app/packages/hoppscotch-sh-admin/dist-subpath-access /site/sh-admin-subpath-access
COPY aio-multiport-setup.Caddyfile /etc/caddy/aio-multiport-setup.Caddyfile
COPY aio-subpath-access.Caddyfile /etc/caddy/aio-subpath-access.Caddyfile

RUN npm install -g @import-meta-env/cli

USER hoppuser

ENTRYPOINT [ "tini", "--" ]
COPY --chmod=755 healthcheck.sh .
HEALTHCHECK --interval=2s CMD /bin/sh ./healthcheck.sh

WORKDIR /dist/backend

CMD ["node", "/usr/src/app/aio_run.mjs"]
EXPOSE 3170
EXPOSE 3000
EXPOSE 3100
EXPOSE 80
