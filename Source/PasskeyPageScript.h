#pragma once

// Grazer as the page's passkey authenticator. Replaces
// navigator.credentials.create and get for publicKey requests (everything else
// goes to WebKit untouched), posts each request to native on the
// grazerPasskey channel as {requestId, operation, publicKey} with bytes in
// base64url, and settles it when native calls window.__grazerPasskeyReply with
// {requestId, credential} or {requestId, error: {name, message}}. The page
// never names its own origin: native takes it from the frame that posted.
static constexpr auto passkeyScriptSource = R"JS(
(() => {
    if (window.__grazerPasskeysInstalled)
        return;

    const channel = window.webkit?.messageHandlers?.grazerPasskey;
    const credentials = navigator.credentials;

    if (!channel || !credentials)
        return;

    Object.defineProperty(window, '__grazerPasskeysInstalled', {value: true});

    const post = message => channel.postMessage(JSON.stringify(message));

    const bytesOf = source => {
        if (ArrayBuffer.isView(source))
            return new Uint8Array(source.buffer, source.byteOffset, source.byteLength);
        return new Uint8Array(source);
    };

    const toBase64Url = source => {
        const bytes = bytesOf(source);
        let binary = '';
        for (const byte of bytes)
            binary += String.fromCharCode(byte);
        return btoa(binary).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
    };

    const fromBase64Url = text => {
        const base64 = text.replace(/-/g, '+').replace(/_/g, '/');
        const base64QuantumLength = 4;
        const missingPadding =
            (base64QuantumLength - base64.length % base64QuantumLength) % base64QuantumLength;
        const binary = atob(base64 + '='.repeat(missingPadding));
        const bytes = new Uint8Array(binary.length);
        for (let index = 0; index < binary.length; index++)
            bytes[index] = binary.charCodeAt(index);
        return bytes.buffer;
    };

    // WebKit's own classes when it has them, so instanceof holds; stand-ins
    // with the same names when it does not.
    const findOrDefineClass = name => {
        if (typeof window[name] !== 'function') {
            const standIn = class {};
            Object.defineProperty(standIn, 'name', {value: name});
            Object.defineProperty(window, name,
                {value: standIn, writable: true, configurable: true});
        }
        return window[name];
    };

    const PublicKeyCredentialClass = findOrDefineClass('PublicKeyCredential');
    const AttestationResponseClass = findOrDefineClass('AuthenticatorAttestationResponse');
    const AssertionResponseClass = findOrDefineClass('AuthenticatorAssertionResponse');

    // Own data properties shadow the prototype's getters, which would throw on
    // an object WebKit did not make.
    const defineValues = (target, values) => {
        for (const [name, value] of Object.entries(values))
            Object.defineProperty(target, name, {value, enumerable: true});
        return target;
    };

    const describeCredentials = descriptors =>
        (descriptors ?? []).map(descriptor => ({
            type: descriptor.type,
            id: toBase64Url(descriptor.id),
        }));

    const describeCreationOptions = options => ({
        challenge: toBase64Url(options.challenge),
        rp: {id: options.rp?.id, name: options.rp?.name},
        user: {
            id: toBase64Url(options.user.id),
            name: options.user.name,
            displayName: options.user.displayName,
        },
        pubKeyCredParams: (options.pubKeyCredParams ?? [])
            .map(parameter => ({type: parameter.type, alg: parameter.alg})),
        excludeCredentials: describeCredentials(options.excludeCredentials),
        authenticatorSelection: {
            userVerification:
                options.authenticatorSelection?.userVerification ?? 'preferred',
        },
    });

    const describeAssertionOptions = options => ({
        challenge: toBase64Url(options.challenge),
        rpId: options.rpId,
        allowCredentials: describeCredentials(options.allowCredentials),
        userVerification: options.userVerification ?? 'preferred',
    });

    const abortReason = signal =>
        signal.reason ?? new DOMException('The operation was aborted.', 'AbortError');

    let nextRequestId = 1;
    const pendingRequests = new Map();

    const askNative = (operation, publicKey, signal) => new Promise((resolve, reject) => {
        if (signal?.aborted) {
            reject(abortReason(signal));
            return;
        }

        const requestId = nextRequestId++;
        pendingRequests.set(requestId, {resolve, reject});

        signal?.addEventListener('abort', () => {
            if (!pendingRequests.delete(requestId))
                return;
            post({requestId, operation: 'abort'});
            reject(abortReason(signal));
        }, {once: true});

        post({requestId, operation, publicKey});
    });

    Object.defineProperty(window, '__grazerPasskeyReply', {
        value: reply => {
            const request = pendingRequests.get(reply.requestId);

            if (!request)
                return;

            pendingRequests.delete(reply.requestId);

            if (reply.error) {
                request.reject(new DOMException(reply.error.message, reply.error.name));
                return;
            }

            request.resolve(reply.credential);
        },
    });

    const buildCredential = (id, response, responseJSON) => {
        const credential = Object.create(PublicKeyCredentialClass.prototype);
        const json = {
            id,
            rawId: id,
            type: 'public-key',
            authenticatorAttachment: 'platform',
            clientExtensionResults: {},
            response: responseJSON,
        };

        return defineValues(credential, {
            id,
            rawId: fromBase64Url(id),
            type: 'public-key',
            authenticatorAttachment: 'platform',
            response,
            getClientExtensionResults: () => ({}),
            toJSON: () => json,
        });
    };

    const buildCreatedCredential = reply => {
        const wire = reply.response;
        const response = defineValues(
            Object.create(AttestationResponseClass.prototype), {
                clientDataJSON: fromBase64Url(wire.clientDataJSON),
                attestationObject: fromBase64Url(wire.attestationObject),
                getTransports: () => [...wire.transports],
                getPublicKey: () => fromBase64Url(wire.publicKey),
                getPublicKeyAlgorithm: () => wire.publicKeyAlgorithm,
                getAuthenticatorData: () => fromBase64Url(wire.authenticatorData),
            });

        return buildCredential(reply.id, response, {...wire});
    };

    const buildAssertedCredential = reply => {
        const wire = reply.response;
        const response = defineValues(
            Object.create(AssertionResponseClass.prototype), {
                clientDataJSON: fromBase64Url(wire.clientDataJSON),
                authenticatorData: fromBase64Url(wire.authenticatorData),
                signature: fromBase64Url(wire.signature),
                userHandle: wire.userHandle ? fromBase64Url(wire.userHandle) : null,
            });

        return buildCredential(reply.id, response, {...wire});
    };

    // Grazer offers no passkey autofill, so a conditional request waits for
    // its abort the way one with no matching passkey would.
    const waitForAbort = signal => new Promise((resolve, reject) => {
        if (signal?.aborted) {
            reject(abortReason(signal));
            return;
        }
        signal?.addEventListener('abort', () => reject(abortReason(signal)), {once: true});
    });

    const originalCreate = credentials.create.bind(credentials);
    const originalGet = credentials.get.bind(credentials);

    credentials.create = async function (options) {
        if (!options?.publicKey)
            return originalCreate(options);

        const publicKey = describeCreationOptions(options.publicKey);
        const reply = await askNative('create', publicKey, options.signal);
        return buildCreatedCredential(reply);
    };

    credentials.get = async function (options) {
        if (!options?.publicKey)
            return originalGet(options);

        if (options.mediation === 'conditional')
            return waitForAbort(options.signal);

        const publicKey = describeAssertionOptions(options.publicKey);
        const reply = await askNative('get', publicKey, options.signal);
        return buildAssertedCredential(reply);
    };

    for (const [name, answer] of [
        ['isUserVerifyingPlatformAuthenticatorAvailable', true],
        ['isConditionalMediationAvailable', false],
    ]) {
        Object.defineProperty(PublicKeyCredentialClass, name, {
            value: () => Promise.resolve(answer),
            writable: true,
            configurable: true,
        });
    }
})();
)JS";
