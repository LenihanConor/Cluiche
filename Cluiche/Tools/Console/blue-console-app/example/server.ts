import * as express from 'express';
import * as cors from 'cors';
import * as path from 'path';

const app = new express();
app.use(cors());

app.post('/api/cli', function (_req, res) {
  res.sendFile(path.join(__dirname, 'static', 'plugin_test_data.json'));
});

app.listen(1400);
