// Reads Grazer's App ID from the Apple developer account, and creates the
// Developer ID provisioning profile Grazer is signed with.
//   node scripts/apple-developer-account.mjs show
//   node scripts/apple-developer-account.mjs create-profile <output path>
// The App Store Connect API key comes from APPLE_API_KEY_ID,
// APPLE_API_ISSUER_ID and APPLE_API_KEY_PATH (the .p8 file).
import { createPrivateKey, sign } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';

const GRAZER_BUNDLE_IDENTIFIER = 'audio.pond.grazer';
const PROFILE_NAME = 'Grazer Developer ID';
const DEVELOPER_ID_PROFILE_TYPE = 'MAC_APP_DIRECT';
const DEVELOPER_ID_CERTIFICATE_TYPES = ['DEVELOPER_ID_APPLICATION', 'DEVELOPER_ID_APPLICATION_G2'];
const API_ORIGIN = 'https://api.appstoreconnect.apple.com';
const TOKEN_LIFETIME_SECONDS = 600;

function readApiKeyFromEnvironment() {
  const names = ['APPLE_API_KEY_ID', 'APPLE_API_ISSUER_ID', 'APPLE_API_KEY_PATH'];
  const missing = names.filter(name => !process.env[name]);

  if (missing.length > 0)
    throw new Error(`missing ${missing.join(', ')}`);

  return {
    keyIdentifier: process.env.APPLE_API_KEY_ID,
    issuerIdentifier: process.env.APPLE_API_ISSUER_ID,
    privateKey: createPrivateKey(readFileSync(process.env.APPLE_API_KEY_PATH)),
  };
}

function encodeAsBase64Url(value) {
  return Buffer.from(value).toString('base64url');
}

function createToken(apiKey) {
  const issuedAt = Math.floor(Date.now() / 1000);
  const header = { alg: 'ES256', kid: apiKey.keyIdentifier, typ: 'JWT' };
  const claims = {
    iss: apiKey.issuerIdentifier,
    iat: issuedAt,
    exp: issuedAt + TOKEN_LIFETIME_SECONDS,
    aud: 'appstoreconnect-v1',
  };
  const unsigned = `${encodeAsBase64Url(JSON.stringify(header))}.${encodeAsBase64Url(JSON.stringify(claims))}`;
  const signature = sign('sha256', Buffer.from(unsigned), { key: apiKey.privateKey, dsaEncoding: 'ieee-p1363' });
  return `${unsigned}.${signature.toString('base64url')}`;
}

async function callApi(apiKey, path, requestBody) {
  const response = await fetch(new URL(path, API_ORIGIN), {
    method: requestBody ? 'POST' : 'GET',
    headers: { Authorization: `Bearer ${createToken(apiKey)}`, 'Content-Type': 'application/json' },
    body: requestBody ? JSON.stringify(requestBody) : undefined,
  });
  const body = await response.json();

  if (!response.ok) {
    const details = (body.errors ?? []).map(error => `${error.code}: ${error.detail}`).join('; ');
    throw new Error(`${response.status} ${path}: ${details}`);
  }

  return body;
}

async function fetchGrazerAppId(apiKey) {
  const body = await callApi(apiKey,
    `/v1/bundleIds?filter[identifier]=${GRAZER_BUNDLE_IDENTIFIER}&include=bundleIdCapabilities,profiles`);
  const appId = (body.data ?? []).find(item => item.attributes.identifier === GRAZER_BUNDLE_IDENTIFIER);

  if (!appId)
    throw new Error(`no App ID ${GRAZER_BUNDLE_IDENTIFIER} in this developer account`);

  return { appId, included: body.included ?? [] };
}

async function fetchDeveloperIdCertificates(apiKey) {
  const body = await callApi(apiKey, '/v1/certificates?limit=200');
  return body.data.filter(item => DEVELOPER_ID_CERTIFICATE_TYPES.includes(item.attributes.certificateType));
}

async function showAccount(apiKey) {
  const { appId, included } = await fetchGrazerAppId(apiKey);
  console.log(`App ID ${appId.attributes.identifier} (${appId.attributes.seedId})`);

  for (const item of included) {
    const { capabilityType, name, profileType, profileState, expirationDate } = item.attributes;
    console.log(' ', item.type, capabilityType ?? `${name} ${profileType} ${profileState} expires ${expirationDate}`);
  }

  for (const certificate of await fetchDeveloperIdCertificates(apiKey)) {
    const { name, serialNumber, expirationDate } = certificate.attributes;
    console.log(`  certificate ${name} serial ${serialNumber} expires ${expirationDate}`);
  }
}

async function createProfile(apiKey, outputPath) {
  const { appId } = await fetchGrazerAppId(apiKey);
  const certificates = await fetchDeveloperIdCertificates(apiKey);

  if (certificates.length === 0)
    throw new Error('this developer account has no Developer ID Application certificate');

  const created = await callApi(apiKey, '/v1/profiles', {
    data: {
      type: 'profiles',
      attributes: { name: PROFILE_NAME, profileType: DEVELOPER_ID_PROFILE_TYPE },
      relationships: {
        bundleId: { data: { type: 'bundleIds', id: appId.id } },
        certificates: { data: certificates.map(certificate => ({ type: 'certificates', id: certificate.id })) },
      },
    },
  });

  writeFileSync(outputPath, Buffer.from(created.data.attributes.profileContent, 'base64'));
  console.log(`Wrote ${outputPath} (${created.data.attributes.name}, expires ${created.data.attributes.expirationDate})`);
}

const [command, outputPath] = process.argv.slice(2);
const apiKey = readApiKeyFromEnvironment();

if (command === 'show')
  await showAccount(apiKey);
else if (command === 'create-profile' && outputPath)
  await createProfile(apiKey, outputPath);
else
  throw new Error('usage: apple-developer-account.mjs show | create-profile <output path>');
