// Shared types used by both rest and rest-request-response modules
// to avoid circular dependencies

import { z } from "zod"

// Basic auth types (from rest/v/1.ts)
export const HoppRESTAuthNone = z.object({
  authType: z.literal("none"),
})

export const HoppRESTAuthInherit = z.object({
  authType: z.literal("inherit"),
})

export const HoppRESTAuthBasic = z.object({
  authType: z.literal("basic"),
  username: z.string().catch(""),
  password: z.string().catch(""),
})

export const HoppRESTAuthBearer = z.object({
  authType: z.literal("bearer"),
  token: z.string().catch(""),
})

// API Key auth (from rest/v/4.ts)
export const HoppRESTAuthAPIKey = z.object({
  authType: z.literal("api-key"),
  key: z.string().catch(""),
  value: z.string().catch(""),
  addTo: z.enum(["HEADERS", "QUERY_PARAMS"]).catch("HEADERS"),
})

// OAuth2 auth types (from rest/v/3.ts)
export const AuthCodeGrantTypeParams = z.object({
  grantType: z.literal("AUTHORIZATION_CODE"),
  authEndpoint: z.string().trim(),
  tokenEndpoint: z.string().trim(),
  clientID: z.string().trim(),
  clientSecret: z.string().trim().optional(),
  scopes: z.string().trim().optional(),
  token: z.string().catch(""),
  isPKCE: z.boolean(),
  codeVerifierMethod: z
    .union([z.literal("plain"), z.literal("S256")])
    .optional(),
  refreshToken: z.string().optional(),
})

export const ClientCredentialsGrantTypeParams = z.object({
  grantType: z.literal("CLIENT_CREDENTIALS"),
  authEndpoint: z.string().trim(),
  clientID: z.string().trim(),
  clientSecret: z.string().trim().optional(),
  scopes: z.string().trim().optional(),
  token: z.string().catch(""),
  clientAuthentication: z.enum(["AS_BASIC_AUTH_HEADERS", "IN_BODY"]),
})

export const PasswordGrantTypeParams = z.object({
  grantType: z.literal("PASSWORD"),
  authEndpoint: z.string().trim(),
  clientID: z.string().trim(),
  clientSecret: z.string().trim().optional(),
  scopes: z.string().trim().optional(),
  username: z.string().trim(),
  password: z.string().trim(),
  token: z.string().catch(""),
})

export const ImplicitOauthFlowParams = z.object({
  grantType: z.literal("IMPLICIT"),
  authEndpoint: z.string().trim(),
  clientID: z.string().trim(),
  scopes: z.string().trim().optional(),
  token: z.string().catch(""),
})

export const HoppRESTAuthOAuth2 = z.object({
  authType: z.literal("oauth-2"),
  grantTypeInfo: z.discriminatedUnion("grantType", [
    AuthCodeGrantTypeParams,
    ClientCredentialsGrantTypeParams,
    PasswordGrantTypeParams,
    ImplicitOauthFlowParams,
  ]),
  addTo: z.enum(["HEADERS", "QUERY_PARAMS"]).catch("HEADERS"),
})

// AWS Signature auth (from rest/v/7.ts)
export const HoppRESTAuthAWSSignature = z.object({
  authType: z.literal("aws-signature"),
  accessKey: z.string().catch(""),
  secretKey: z.string().catch(""),
  region: z.string().catch(""),
  serviceName: z.string().catch(""),
  serviceToken: z.string().optional(),
  signature: z.object({}).optional(),
  addTo: z.enum(["HEADERS", "QUERY_PARAMS"]).catch("HEADERS"),
})

// Digest auth (from rest/v/8.ts)
export const HoppRESTAuthDigest = z.object({
  authType: z.literal("digest"),
  username: z.string().catch(""),
  password: z.string().catch(""),
})

// HAWK auth (from rest/v/9.ts)
export const HoppRESTAuthHAWK = z.object({
  authType: z.literal("hawk"),
  id: z.string().catch(""),
  key: z.string().catch(""),
  algorithm: z.enum(["sha1", "sha256"]).catch("sha1"),
  user: z.string().optional(),
  ext: z.string().optional(),
  dlg: z.string().optional(),
})

// Akamai EdgeGrid auth (from rest/v/10.ts)
export const HoppRESTAuthAkamaiEdgeGrid = z.object({
  authType: z.literal("akamai-edgegrid"),
  accessToken: z.string().catch(""),
  clientToken: z.string().catch(""),
  clientSecret: z.string().catch(""),
  baseURL: z.string().catch(""),
})

// JWT auth (from rest/v/13.ts)
export const HoppRESTAuthJWT = z.object({
  authType: z.literal("jwt"),
  secret: z.string().catch(""),
  privateKey: z.string().catch(""),
  algorithm: z
    .enum([
      "HS256",
      "HS384", 
      "HS512",
      "RS256",
      "RS384",
      "RS512",
      "PS256",
      "PS384",
      "PS512",
      "ES256",
      "ES384",
      "ES512",
    ])
    .catch("HS256"),
  payload: z.string().catch("{}"),
})

// Combined auth type for the latest version
export const HoppRESTAuth = z
  .discriminatedUnion("authType", [
    HoppRESTAuthNone,
    HoppRESTAuthInherit,
    HoppRESTAuthBasic,
    HoppRESTAuthBearer,
    HoppRESTAuthOAuth2,
    HoppRESTAuthAPIKey,
    HoppRESTAuthAWSSignature,
    HoppRESTAuthDigest,
    HoppRESTAuthHAWK,
    HoppRESTAuthAkamaiEdgeGrid,
    HoppRESTAuthJWT,
  ])
  .and(
    z.object({
      authActive: z.boolean(),
    })
  )

export type HoppRESTAuth = z.infer<typeof HoppRESTAuth>

// Request body types (from rest/v/10.ts)
export const FormDataKeyValue = z
  .object({
    key: z.string(),
    active: z.boolean(),
    contentType: z.string().optional().catch(undefined),
  })
  .and(
    z.union([
      z.object({
        isFile: z.literal(true),
        value: z.array(z.instanceof(Blob).nullable()).catch([]),
      }),
      z.object({
        isFile: z.literal(false),
        value: z.string(),
      }),
    ])
  )
  .transform((data) => {
    if (data.isFile && Array.isArray(data.value) && data.value.length === 0) {
      return {
        ...data,
        isFile: false,
        value: "",
      }
    }
    return data
  })

export const HoppRESTReqBody = z.union([
  z.object({
    contentType: z.literal(null),
    body: z.literal(null).catch(null),
  }),
  z.object({
    contentType: z.literal("multipart/form-data"),
    body: z.array(FormDataKeyValue).catch([]),
    showIndividualContentType: z.boolean().optional().catch(false),
    isBulkEditing: z.boolean().optional().catch(false),
  }),
  z.object({
    contentType: z.literal("application/octet-stream"),
    body: z.instanceof(File).nullable().catch(null),
  }),
  z.object({
    contentType: z.literal("application/x-www-form-urlencoded"),
    body: z.string().catch(""),
    isBulkEditing: z.boolean().optional().catch(false),
  }),
  z.object({
    contentType: z.union([
      z.literal("application/json"),
      z.literal("application/ld+json"),
      z.literal("application/hal+json"),
      z.literal("application/vnd.api+json"),
      z.literal("application/xml"),
      z.literal("text/xml"),
      z.literal("binary"),
      z.literal("text/html"),
      z.literal("text/plain"),
    ]),
    body: z.string().catch(""),
  }),
])

export type HoppRESTReqBody = z.infer<typeof HoppRESTReqBody>