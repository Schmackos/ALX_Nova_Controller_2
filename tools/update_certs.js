#!/usr/bin/env node
// tools/update_certs.js — Update OTA root CA certificates
// Usage: node tools/update_certs.js
//
// Connects to GitHub API and CDN endpoints to extract the current
// TLS certificate chain, then regenerates src/ota_certs.h with
// the root CA certificates.

const tls = require('tls');
const https = require('https');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const HOSTS = ['api.github.com', 'objects.githubusercontent.com'];
const OUTPUT = path.join(__dirname, '..', 'src', 'ota_certs.h');

/**
 * Connect to a host and extract the full certificate chain.
 * Returns an array of { pem, subject, issuer, validFrom, validTo, serialNumber, fingerprint }.
 */
function getCertChain(host) {
    return new Promise((resolve, reject) => {
        const socket = tls.connect(443, host, { servername: host }, () => {
            try {
                const peerCert = socket.getPeerCertificate(true);
                if (!peerCert || !peerCert.raw) {
                    socket.end();
                    return reject(new Error(`No certificate returned by ${host}`));
                }

                const chain = [];
                const seen = new Set();
                let cert = peerCert;

                while (cert && cert.raw) {
                    const fingerprint = crypto.createHash('sha256')
                        .update(cert.raw)
                        .digest('hex');

                    if (seen.has(fingerprint)) break;
                    seen.add(fingerprint);

                    const pem = '-----BEGIN CERTIFICATE-----\n' +
                        cert.raw.toString('base64').match(/.{1,64}/g).join('\n') +
                        '\n-----END CERTIFICATE-----';

                    chain.push({
                        pem,
                        subject: formatDN(cert.subject),
                        issuer: formatDN(cert.issuer),
                        validFrom: cert.valid_from,
                        validTo: cert.valid_to,
                        serialNumber: cert.serialNumber,
                        fingerprint,
                        isSelfSigned: formatDN(cert.subject) === formatDN(cert.issuer)
                    });

                    cert = cert.issuerCertificate;
                }

                socket.end();
                resolve({ host, chain });
            } catch (err) {
                socket.end();
                reject(err);
            }
        });

        socket.setTimeout(10000);
        socket.on('timeout', () => {
            socket.destroy();
            reject(new Error(`Connection to ${host} timed out`));
        });
        socket.on('error', reject);
    });
}

/**
 * Format a certificate distinguished name object into a readable string.
 */
function formatDN(dnObj) {
    if (!dnObj) return '(unknown)';
    const parts = [];
    if (dnObj.CN) parts.push(`CN=${dnObj.CN}`);
    if (dnObj.O) parts.push(`O=${dnObj.O}`);
    if (dnObj.C) parts.push(`C=${dnObj.C}`);
    return parts.join(', ') || JSON.stringify(dnObj);
}

/**
 * Generate the C++ header file content.
 */
function generateHeader(roots) {
    const now = new Date().toISOString().split('T')[0];
    let header = `#pragma once
// OTA Root CA Certificates for GitHub API / CDN
// Last updated: ${now}
// Update procedure: Run \`node tools/update_certs.js\`
// See docs-site/docs/developer/ota-certs.md for details
//
`;

    roots.forEach((root, i) => {
        header += `// Certificate ${i + 1}: ${root.subject}\n`;
        header += `//   Valid until: ${root.validTo}\n`;
        header += `//   Covers: ${root.hosts.join(', ')}\n`;
        header += `//   Serial: ${root.serialNumber}\n`;
        header += `//   SHA-256: ${root.fingerprint.substring(0, 16)}...\n`;
        header += `//\n`;
    });

    header += `\nstatic const char* GITHUB_ROOT_CA = \\\n`;

    roots.forEach((root, i) => {
        const lines = root.pem.split('\n');
        lines.forEach((line, j) => {
            const isLast = (i === roots.length - 1) && (j === lines.length - 1);
            header += `"${line}\\n"`;
            if (!isLast) {
                header += ` \\\n`;
            }
        });
        // Add separator newline between certs (the \\n at end of each line handles PEM format)
        if (i < roots.length - 1) {
            // No extra separator needed — the PEM END + BEGIN lines handle it
        }
    });

    header += `;\n`;
    return header;
}

async function main() {
    console.log('OTA Certificate Updater');
    console.log('======================\n');

    // Fetch certificate chains from all hosts
    const results = [];
    for (const host of HOSTS) {
        console.log(`Connecting to ${host}...`);
        try {
            const result = await getCertChain(host);
            results.push(result);

            console.log(`  Chain depth: ${result.chain.length} certificates`);
            result.chain.forEach((cert, i) => {
                const prefix = cert.isSelfSigned ? '  [ROOT]' : '       ';
                console.log(`${prefix} ${i + 1}. ${cert.subject}`);
                console.log(`         Issuer: ${cert.issuer}`);
                console.log(`         Valid: ${cert.validFrom} — ${cert.validTo}`);
                console.log(`         Serial: ${cert.serialNumber}`);
            });
            console.log();
        } catch (err) {
            console.error(`  ERROR: ${err.message}`);
            process.exit(1);
        }
    }

    // Extract root CAs (self-signed certs), deduplicate by fingerprint
    const rootMap = new Map();
    for (const result of results) {
        for (const cert of result.chain) {
            if (cert.isSelfSigned) {
                if (rootMap.has(cert.fingerprint)) {
                    rootMap.get(cert.fingerprint).hosts.push(result.host);
                } else {
                    rootMap.set(cert.fingerprint, {
                        ...cert,
                        hosts: [result.host]
                    });
                }
            }
        }
    }

    const roots = Array.from(rootMap.values());

    if (roots.length === 0) {
        console.error('ERROR: No root CA certificates found in any chain.');
        console.error('This may indicate the hosts are using intermediate-only chains.');
        process.exit(1);
    }

    console.log(`Found ${roots.length} unique root CA(s):`);
    roots.forEach((root, i) => {
        console.log(`  ${i + 1}. ${root.subject}`);
        console.log(`     Valid until: ${root.validTo}`);
        console.log(`     Hosts: ${root.hosts.join(', ')}`);
    });
    console.log();

    // Read existing file for comparison
    let changed = true;
    if (fs.existsSync(OUTPUT)) {
        const existing = fs.readFileSync(OUTPUT, 'utf8');
        const newContent = generateHeader(roots);

        // Compare PEM content only (ignore date/comment changes)
        const extractPEM = (s) => {
            const matches = s.match(/-----BEGIN CERTIFICATE-----[\s\S]*?-----END CERTIFICATE-----/g);
            return matches ? matches.join('\n') : '';
        };

        if (extractPEM(existing) === extractPEM(newContent)) {
            changed = false;
            console.log('No certificate changes detected. File is up to date.');
        } else {
            console.log('Certificate changes detected!');
        }
    }

    // Generate and write new header
    const content = generateHeader(roots);
    fs.writeFileSync(OUTPUT, content, 'utf8');
    console.log(`\nWrote ${OUTPUT}`);

    if (changed) {
        console.log('\nIMPORTANT: Certificates have been updated.');
        console.log('Please rebuild the firmware and test OTA before releasing:');
        console.log('  pio run');
        console.log('  pio test -e native -f test_ota');
    }

    // Warn about upcoming expirations (within 1 year)
    const oneYearFromNow = new Date();
    oneYearFromNow.setFullYear(oneYearFromNow.getFullYear() + 1);
    for (const root of roots) {
        const expiry = new Date(root.validTo);
        if (expiry < oneYearFromNow) {
            console.log(`\nWARNING: Certificate "${root.subject}" expires on ${root.validTo}`);
            console.log('  This certificate should be replaced soon!');
        }
    }
}

main().catch(err => {
    console.error(`Fatal error: ${err.message}`);
    process.exit(1);
});
