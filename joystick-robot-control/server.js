const WebSocket = require('ws');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

// Set up WebSocket server on port 8080
const wss = new WebSocket.Server({ port: 8080 });

// Set up SerialPort to communicate with Arduino
const port = new SerialPort({
    path: 'COM3', // Update this to match your Arduino's port
    baudRate: 9600,
});

// Set up a parser to read lines from the serial port
const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

port.on('open', () => {
    console.log('Serial port opened');
});

port.on('error', (err) => {
    console.error('Serial port error:', err.message);
});

// Handle position updates from the Arduino
parser.on('data', (data) => {
    console.log('Received from Arduino:', data);
    if (data.startsWith('POS:')) {
        const position = parseInt(data.substring(4));
        // Broadcast the position to all connected WebSocket clients
        wss.clients.forEach((client) => {
            if (client.readyState === WebSocket.OPEN) {
                client.send(JSON.stringify({ position: position }));
            }
        });
    } else if (data.startsWith('SPEED:')) {
        const speed = parseInt(data.substring(6));
        // Broadcast the speed to all connected WebSocket clients
        wss.clients.forEach((client) => {
            if (client.readyState === WebSocket.OPEN) {
                client.send(JSON.stringify({ speed: speed }));
            }
        });
    }
});

// Handle WebSocket connections from the browser
wss.on('connection', (ws) => {
    console.log('WebSocket client connected');

    ws.on('message', (message) => {
        const data = JSON.parse(message);
        console.log('Received from browser:', data);

        if (data.command === 'reset') {
            // Send reset command to Arduino
            port.write('RESET\n', (err) => {
                if (err) {
                    console.error('Error writing to serial port:', err.message);
                } else {
                    console.log('Sent reset command to Arduino');
                }
            });
        } else if (data.command === 'move') {
            // Send move command to Arduino with the target position
            const targetPosition = data.position;
            port.write(`MOVE ${targetPosition}\n`, (err) => {
                if (err) {
                    console.error('Error writing to serial port:', err.message);
                } else {
                    console.log(`Sent move command to Arduino: MOVE ${targetPosition}`);
                }
            });
        } else if (data.command === 'jog_cw') {
            // Check if speed parameter is provided
            if (data.speed !== undefined) {
                // Send jog command with speed parameter
                port.write(`JOG_CW ${data.speed}\n`, (err) => {
                    if (err) {
                        console.error('Error writing to serial port:', err.message);
                    } else {
                        console.log(`Sent jog clockwise command with speed ${data.speed} to Arduino`);
                    }
                });
            } else {
                // Send regular jog command (use max speed)
                port.write('JOG_CW\n', (err) => {
                    if (err) {
                        console.error('Error writing to serial port:', err.message);
                    } else {
                        console.log('Sent jog clockwise command to Arduino');
                    }
                });
            }
        } else if (data.command === 'jog_ccw') {
            // Check if speed parameter is provided
            if (data.speed !== undefined) {
                // Send jog command with speed parameter
                port.write(`JOG_CCW ${data.speed}\n`, (err) => {
                    if (err) {
                        console.error('Error writing to serial port:', err.message);
                    } else {
                        console.log(`Sent jog counterclockwise command with speed ${data.speed} to Arduino`);
                    }
                });
            } else {
                // Send regular jog command (use max speed)
                port.write('JOG_CCW\n', (err) => {
                    if (err) {
                        console.error('Error writing to serial port:', err.message);
                    } else {
                        console.log('Sent jog counterclockwise command to Arduino');
                    }
                });
            }
        } else if (data.command === 'stop_jog') {
            port.write('STOP_JOG\n', (err) => {
                if (err) {
                    console.error('Error writing to serial port:', err.message);
                } else {
                    console.log('Sent stop jog command to Arduino');
                }
            });
        }
    });

    ws.on('close', () => {
        console.log('WebSocket client disconnected');
    });
});

console.log('WebSocket server running on ws://localhost:8080');