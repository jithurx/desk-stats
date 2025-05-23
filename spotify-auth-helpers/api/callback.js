// File: api/callback.js

export default function handler(req, res) {
  // Vercel automatically parses query parameters into req.query
  const { code, error, state } = req.query;

  let htmlResponse;

  if (error) {
    htmlResponse = `
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Spotify OAuth Error</title>
        <style>
          body { font-family: sans-serif; margin: 20px; background-color: #f4f4f4; color: #333; }
          .container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
          h1 { color: #dc3545; }
          p { font-size: 1.1em; }
          .code-block {
            background-color: #e9ecef;
            padding: 10px;
            border-radius: 4px;
            font-family: monospace;
            word-break: break-all;
            margin-top: 10px;
          }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>Spotify Authorization Failed</h1>
          <p>An error occurred during the Spotify authorization process:</p>
          <div class="code-block">${encodeURIComponent(error)}</div>
          ${state ? `<p>State (if provided): <span class="code-block">${encodeURIComponent(state)}</span></p>` : ''}
          <p>Please check the error and try authorizing again through your script.</p>
        </div>
      </body>
      </html>
    `;
    res.status(400).send(htmlResponse); // Send 400 for error
  } else if (code) {
    htmlResponse = `
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Spotify Authorization Success</title>
        <style>
          body { font-family: sans-serif; margin: 20px; background-color: #f4f4f4; color: #333; }
          .container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
          h1 { color: #28a745; }
          p { font-size: 1.1em; }
          .code-block {
            background-color: #e9ecef;
            padding: 15px;
            border-radius: 4px;
            font-family: monospace;
            font-size: 1.2em; /* Make code bigger */
        word-break: break-all;
            margin-top: 10px;
            border: 1px solid #ccc;
          }
          .copy-button {
            display: inline-block;
            padding: 8px 15px;
            margin-top: 10px;
            background-color: #007bff;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 0.9em;
          }
          .copy-button:hover {
            background-color: #0056b3;
          }
          #copy-status {
            margin-left: 10px;
            font-style: italic;
            color: #28a745;
          }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>Spotify Authorization Successful!</h1>
          <p>Your authorization code is:</p>
          <div id="authCode" class="code-block">${code}</div>
          <button class="copy-button" onclick="copyCode()">Copy Code</button>
          <span id="copy-status"></span>
          ${state ? `<p>State (if provided): <span class="code-block">${encodeURIComponent(state)}</span></p>` : ''}
          <p>Please <strong>copy the code above</strong> and paste it back into your Python script when prompted.</p>
          <p>You can now close this browser tab.</p>
        </div>
        <script>
          function copyCode() {
            const codeElement = document.getElementById('authCode');
            const range = document.createRange();
            range.selectNode(codeElement);
            window.getSelection().removeAllRanges(); // Clear previous selections
            window.getSelection().addRange(range); // To select text
            try {
              document.execCommand('copy');
              document.getElementById('copy-status').textContent = 'Copied!';
            } catch (err) {
              document.getElementById('copy-status').textContent = 'Failed to copy.';
              console.error('Failed to copy text: ', err);
            }
            window.getSelection().removeAllRanges(); // Deselect
          }
        </script>
      </body>
      </html>
    `;
    res.status(200).send(htmlResponse);
  } else {
    htmlResponse = `
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Spotify OAuth Callback</title>
         <style>body { font-family: sans-serif; margin: 20px; }</style>
      </head>
      <body>
        <h1>Spotify OAuth Callback Endpoint</h1>
        <p>This page is intended to receive a callback from Spotify after authorization.</p>
        <p>If you see this, it means Spotify hasn't redirected here with a code or error yet.</p>
      </body>
      </html>
    `;
    res.status(200).send(htmlResponse); // Or 400 if you expect a code always
  }
}